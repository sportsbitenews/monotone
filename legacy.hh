#ifndef __LEGACY_HH__
#define __LEGACY_HH__

// copyright (C) 2005 nathaniel smith <njs@pobox.com>
// all rights reserved.
// licensed to the public under the terms of the GNU GPL (>= 2)
// see the file COPYING for details

// old code needed for reading legacy data (so we can then convert it)

#include <map>
#include <string>

#include "paths.hh"

namespace legacy
{
  ////////
  // parser for old .mt-attrs file format
  typedef std::map<file_path, std::map<std::string, std::string> > dot_mt_attrs_map;

  void 
  read_dot_mt_attrs(data const & dat, dot_mt_attrs_map & attr);

  ///////
  // parsing old-style revisions, for 'rosterify' command

  // HACK: this is a special reader which picks out the new_manifest field in
  // a revision; it ignores all other symbols. This is because, in the
  // pre-roster database, we have revisions holding change_sets, not
  // csets. If we apply the cset reader to them, they fault. We need to
  // *partially* read them, however, in order to get the manifest IDs out of
  // the old revisions (before we delete the revs and rebuild them)

  typedef std::map<revision_id, std::map<split_path, split_path> > renames_map;

  void 
  get_manifest_and_renames_for_rev(app_state & app,
                                   revision_id const & ident,
                                   manifest_id & mid,
                                   renames_map & renames);

  ///////
  // parsing old-style manifests, for 'rosterify' and 'changesetify' commands
  typedef std::map<file_path, file_id, 
                   std::less<file_path> > manifest_map;
  void read_manifest_map(manifest_data const & mdat,
                         manifest_map & man);

}

#endif