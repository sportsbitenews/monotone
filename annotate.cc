// copyright (C) 2005 emile snyder <emile@alumni.reed.edu>
// all rights reserved.
// licensed to the public under the terms of the GNU GPL (>= 2)
// see the file COPYING for details

#include <string>
#include <sstream>
#include <deque>
#include <set>

#include <boost/shared_ptr.hpp>

#include "platform.hh"
#include "vocab.hh"
#include "sanity.hh"
#include "revision.hh"
#include "change_set.hh"
#include "app_state.hh"
#include "manifest.hh"
#include "transforms.hh"
#include "lcs.hh"
#include "annotate.hh"


/* 
   file of interest, 'foo', is made up of 6 lines, while foo's 
   parent (foo') is 5 lines:

   foo     foo'
   A       A
   B       z
   C       B
   D       C
   E       y
   F

   The longest common subsequence between foo and foo' is
   [A,B,C] and we know that foo' lines map to foo lines like
   so:
   
   foo'
   A    0 -> 0
   z    1 -> (none)
   B    2 -> 1
   C    3 -> 2
   y    4 -> (none)
   
   How do we know?  Because we walk the file along with the LCS,
   having initialized the copy_count at 0, and do:

   i = j = copy_count = 0;
   while (i < foo'.size()) {
     map[i] = -1;
     if (foo'[i] == LCS[j]) { 
       map[i] = lcs_src_lines[j];
       i++; j++; copy_count++;
       continue;
     }
     i++;
   }

   If we're trying to annotate foo, we want to assign each line
   of foo that we can't find in the LCS to the foo revision (since 
   it can't have come from further back in time.)  So at each edge
   we do the following:

   1. build the LCS
   2. walk over the child (foo) and the LCS simultaneously, using the
      lineage map of the child and the LCS to assign
      blame as we go for lines that aren't in the LCS.  Also generate
      a vector, lcs_src_lines, with the same length as LCS whose 
      elements are the line in foo which that LCS entry represents. 
      So for foo, it would be [0, 1, 2] because [A,B,C] is the first
      3 elements.
   3. walk over the parent (foo'), using our exising lineage map and the 
      LCS, to build the parent's lineage map (which will be used
      at the next phase.)

*/   

class annotate_lineage_mapping;

class annotate_context {
public:
  annotate_context(file_id fid, app_state &app);

  boost::shared_ptr<annotate_lineage_mapping> initial_lineage() const;

  /// credit any remaining unassigned lines to rev
  void complete(revision_id rev);

  /// credit any uncopied lines (as recorded in copied_lines) to
  /// rev, and reset copied_lines.
  void evaluate(revision_id rev);

  void set_copied(int index);

  void set_root_revision(revision_id rev) { root_revision = rev; }
  revision_id get_root_revision() const { return root_revision; }

  /// return an immutable reference to our vector of string data for external use
  const std::vector<std::string>& get_file_lines() const;

  /// return true if we have no more unassigned lines
  bool is_complete() const;

  void dump() const;

private:
  std::vector<std::string> file_lines;
  std::vector<long> file_interned;
  std::vector<revision_id> annotations;

  // given a node and it's set of parents, we need to keep track
  // of which of our lines was copied back by *some* node -> parent
  // transition. we do it here.
  std::vector<bool> copied_lines;

  revision_id root_revision;
};



/*
  An annotate_lineage_mapping tells you, for each line of a file, where in the 
  ultimate descendent of interest (UDOI) the line came from (a line not
  present in the UDOI is represented as -1).
*/
class annotate_lineage_mapping {
public:
  annotate_lineage_mapping(const file_data &data);
  annotate_lineage_mapping(const std::vector<std::string> &lines);

  /// need a better name.  does the work of setting copied bits in the context object.
  boost::shared_ptr<annotate_lineage_mapping> 
  build_parent_lineage(boost::shared_ptr<annotate_context> acp, revision_id parent_rev, const file_data &parent_data) const;

private:
  void init_with_lines(const std::vector<std::string> &lines);

  static interner<long> in; // FIX, testing hack 

  //std::vector<std::string> file_lines;
  std::vector<long> file_interned;

  // same length as file_lines. if file_lines[i] came from line 4, mapping[i] = 4
  std::vector<int> mapping;
};


interner<long> annotate_lineage_mapping::in;


// a set of data that specifies the input data needed to process 
// the annotation for a given childrev -> parentrev edge.
struct annotate_node_work {
  annotate_node_work (boost::shared_ptr<annotate_context> annotations_,
                      boost::shared_ptr<annotate_lineage_mapping> lineage_,
                      revision_id node_revision_, file_id node_fid_, file_path node_fpath_)
    : annotations(annotations_), 
      lineage(lineage_),
      node_revision(node_revision_), 
      node_fid(node_fid_), 
      node_fpath(node_fpath_)
  {}

  boost::shared_ptr<annotate_context> annotations;
  boost::shared_ptr<annotate_lineage_mapping> lineage;
  revision_id node_revision;
  file_id     node_fid;
  file_path   node_fpath;
};





annotate_context::annotate_context(file_id fid, app_state &app)
{
  // initialize file_lines
  file_data fpacked;
  app.db.get_file_version(fid, fpacked);
  std::string encoding = default_encoding; // FIXME
  split_into_lines(fpacked.inner()(), encoding, file_lines);
  L(F("annotate_context::annotate_context initialized with %d file lines\n") % file_lines.size());

  // initialize file_interned
  interner<long> in;
  file_interned.clear();
  file_interned.reserve(file_lines.size());
  file_interned.clear();
  for (size_t i = 0; i < file_lines.size(); i++) {
    file_interned.push_back(in.intern(file_lines[i]));
  }
  L(F("annotate_context::annotate_context initialized with %d entries in file_interned\n") % file_interned.size());

  // initialize annotations
  revision_id nullid;
  annotations.clear();
  annotations.reserve(file_lines.size());
  annotations.insert(annotations.begin(), file_lines.size(), nullid);
  L(F("annotate_context::annotate_context initialized with %d entries in annotations\n") % annotations.size());

  // initialize copied_lines
  copied_lines.clear();
  copied_lines.reserve(file_lines.size());
  copied_lines.insert(copied_lines.begin(), file_lines.size(), false);
}


boost::shared_ptr<annotate_lineage_mapping> 
annotate_context::initial_lineage() const
{
  boost::shared_ptr<annotate_lineage_mapping> res(new annotate_lineage_mapping(file_lines));
  return res;
}


void 
annotate_context::complete(revision_id rev)
{
  revision_id nullid;
  std::vector<revision_id>::iterator i;
  for (i=annotations.begin(); i != annotations.end(); i++) {
    if (*i == nullid) 
      *i = rev;
  }
}

void 
annotate_context::evaluate(revision_id rev)
{
  revision_id nullid;
  I(copied_lines.size() == annotations.size());
  for (size_t i=0; i<copied_lines.size(); i++) {
    if (!copied_lines[i] && annotations[i] == nullid) {
      L(F("evaluate setting annotations[%d] -> %s, since copied_lines[%d] was %d and annotations[%d] was %s\n") 
        % i % rev % i % copied_lines[i] % i % annotations[i]);
      annotations[i] = rev;
    } else {
      L(F("evaluate LEAVING annotations[%d] -> %s, since copied_lines[%d] was %d and annotations[%d] was %s\n") 
        % i % annotations[i] % i % copied_lines[i] % i % annotations[i]);
    }

    copied_lines[i] = false; // reset as we go
  }
}

void 
annotate_context::set_copied(int index)
{
  L(F("annotate_context::set_copied %d\n") % index);
  if (index == -1)
    return;

  I(index >= 0 && index < (int)copied_lines.size());
  copied_lines[(size_t)index] = true;
}


const std::vector<std::string>& 
annotate_context::get_file_lines() const
{
  return file_lines;
}


bool
annotate_context::is_complete() const
{
  // FIX: cache value as we walk it each go round.

  revision_id nullid;
  std::vector<revision_id>::const_iterator i;
  for (i=annotations.begin(); i != annotations.end(); i++) {
    if (*i == nullid)
      return false;
  }

  return true;
}


void
annotate_context::dump() const
{
  revision_id nullid;

  std::vector<revision_id>::const_iterator i;
  for (i = annotations.begin(); i != annotations.end(); i++) {
    if (*i == nullid)
      std::cout << "(unassigned)" << std::endl;
    else 
      std::cout << *i << std::endl;
  }
}


annotate_lineage_mapping::annotate_lineage_mapping(const file_data &data)
{
  // split into lines
  std::vector<std::string> lines;
  std::string encoding = default_encoding; // FIXME
  split_into_lines (data.inner()().data(), encoding, lines);

  init_with_lines(lines);
}

annotate_lineage_mapping::annotate_lineage_mapping(const std::vector<std::string> &lines)
{
  init_with_lines(lines);
}

void
annotate_lineage_mapping::init_with_lines(const std::vector<std::string> &lines)
{
  file_interned.clear();
  file_interned.reserve(lines.size());
  mapping.clear();
  mapping.reserve(lines.size());

  //interner<long> in;

  int count;
  std::vector<std::string>::const_iterator i;
  for (count=0, i = lines.begin(); i != lines.end(); i++, count++) {
    file_interned.push_back(in.intern(*i));
    mapping.push_back(count);
  }
  L(F("annotate_lineage_mapping::init_with_lines  ending with %d entries in mapping\n") % mapping.size());
}


boost::shared_ptr<annotate_lineage_mapping>
annotate_lineage_mapping::build_parent_lineage (boost::shared_ptr<annotate_context> acp, 
                                                revision_id parent_rev, 
                                                const file_data &parent_data) const
{
  boost::shared_ptr<annotate_lineage_mapping> parent_lineage(new annotate_lineage_mapping(parent_data));

  std::vector<long> lcs;
  longest_common_subsequence(file_interned.begin(), 
                             file_interned.end(),
                             parent_lineage->file_interned.begin(), 
                             parent_lineage->file_interned.end(),
                             std::min(file_interned.size(), parent_lineage->file_interned.size()),
                             std::back_inserter(lcs));

  L(F("build_parent_lineage: file_lines.size() == %d, parent.file_lines.size() == %d, lcs.size() == %d\n")
    % file_interned.size() % parent_lineage->file_interned.size() % lcs.size());

  // do the copied lines thing for our annotate_context
  std::vector<long> lcs_src_lines;
  lcs_src_lines.reserve(lcs.size());
  size_t i, j;
  i = j = 0;
  while (i < file_interned.size() && j < lcs.size()) {
    L(F("file_interned[%d]: %ld    lcs[%d]: %ld\n") % i % file_interned[i] % j % lcs[j]);

    if (file_interned[i] == lcs[j]) {
      acp->set_copied(mapping[i]);
      lcs_src_lines[j] = mapping[i];
      j++;
    }

    i++;
  }
  L(F("loop ended with i: %d, j: %d, lcs.size(): %d\n") % i % j % lcs.size());
  I(j == lcs.size());

  // determine the mapping for parent lineage
  L(F("build_parent_lineage: building mapping now\n"));
  i = j = 0;
  while (i < parent_lineage->file_interned.size() && j < lcs.size()) {
    if (parent_lineage->file_interned[i] == lcs[j]) {
      parent_lineage->mapping[i] = lcs_src_lines[j];
      j++;
    } else {
      parent_lineage->mapping[i] = -1;
    }
    L(F("mapping[%d] -> %d\n") % i % parent_lineage->mapping[i]);
    
    i++;
  }
  I(j == lcs.size());

  return parent_lineage;
}


static file_id
find_file_id_in_revision(app_state &app, file_path fpath, revision_id rid)
{
  // find the version of the file requested
  manifest_map mm;
  revision_set rev;
  app.db.get_revision(rid, rev);
  app.db.get_manifest(rev.new_manifest, mm);
  manifest_map::const_iterator i = mm.find(fpath);
  I(i != mm.end());
  file_id fid = manifest_entry_id(*i);
  return fid;
}

static void
do_annotate_node (const annotate_node_work &work_unit, 
                  app_state &app,
                  std::deque<annotate_node_work> &nodes_to_process,
                  std::set<revision_id> &nodes_seen)
{
  L(F("do_annotate_node for node %s\n") % work_unit.node_revision);
  nodes_seen.insert(work_unit.node_revision);
  revision_id null_revision; // initialized to 0 by default?

  // get the revisions parents
  std::set<revision_id> parents;
  app.db.get_revision_parents(work_unit.node_revision, parents);
  L(F("do_annotate_node found %d parents for node %s\n") % parents.size() % work_unit.node_revision);

  std::set<revision_id>::const_iterator parent;
  for (parent = parents.begin(); parent != parents.end(); parent++) {
    if (*parent == null_revision) {
      // work_unit.node_revision is the root node
      L(F("do_annotate_node setting root revision as %s\n") % work_unit.node_revision);
      work_unit.annotations->set_root_revision(work_unit.node_revision);
      return;
    }

    change_set cs;
    calculate_arbitrary_change_set (*parent, work_unit.node_revision, app, cs);

    file_path parent_fpath = apply_change_set_inverse(cs, work_unit.node_fpath);
    L(F("file %s in parent revision %s is %s\n") % work_unit.node_fpath % *parent % parent_fpath);

    if (parent_fpath == std::string("")) {
      // I(work_unit.node_revision added work_unit.node_fid)
      L(F("revision %s added file %s (file id %s), terminating annotation processing\n") 
        % work_unit.node_revision % work_unit.node_fpath % work_unit.node_fid);
      work_unit.annotations->complete(work_unit.node_revision);
      return;
    }
    file_id parent_fid = find_file_id_in_revision(app, parent_fpath, *parent);

    boost::shared_ptr<annotate_lineage_mapping> parent_lineage;

    if (! (work_unit.node_fid == parent_fid)) {
      file_data data;
      app.db.get_file_version(parent_fid, data);

      parent_lineage = work_unit.lineage->build_parent_lineage(work_unit.annotations,
                                                               work_unit.node_revision,
                                                               data);
    } else {
      parent_lineage = work_unit.lineage;
    }

    // if this parent has not yet been queued for processing, create the work unit for it.
    if (nodes_seen.find(*parent) == nodes_seen.end()) {
      nodes_seen.insert(*parent);
      annotate_node_work newunit(work_unit.annotations, parent_lineage, *parent, parent_fid, parent_fpath);
      nodes_to_process.push_back(newunit);
    }
  }

  work_unit.annotations->evaluate(work_unit.node_revision);
}


void 
do_annotate (app_state &app, file_path fpath, file_id fid, revision_id rid)
{
  L(F("annotating file %s with id %s in revision %s\n") % fpath % fid % rid);

  // get the file length...
  //file_data fdata;
  //app.db.get_file_version(fid, fdata);
    
  boost::shared_ptr<annotate_context> acp(new annotate_context(fid, app));
  boost::shared_ptr<annotate_lineage_mapping> lineage = acp->initial_lineage();

  // build node work unit
  std::deque<annotate_node_work> nodes_to_process;
  std::set<revision_id> nodes_seen;
  annotate_node_work workunit(acp, lineage, rid, fid, fpath);
  nodes_to_process.push_back(workunit);

  while (nodes_to_process.size() && !acp->is_complete()) {
    annotate_node_work work = nodes_to_process.front();
    nodes_to_process.pop_front();
    do_annotate_node(work, app, nodes_to_process, nodes_seen);
  }
  if (!acp->is_complete()) {
    L(F("do_annotate acp remains incomplete after processing all nodes\n"));
    revision_id null_revision;
    I(!(acp->get_root_revision() == null_revision));
    acp->complete(acp->get_root_revision());
  }

  acp->dump();
  //boost::shared_ptr<annotation_formatter> frmt(new annotation_text_formatter()); 
  //write_annotations(acp, frmt); // automatically write to stdout, or make take a stream argument?
}


/*
void
write_annotations (boost::shared_ptr<annotate_context> acp, 
                   boost::shared_ptr<annotation_formatter> frmt) 
{
}
*/