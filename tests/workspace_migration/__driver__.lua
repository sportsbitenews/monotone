-- This test ensures that 'mtn workspace_migrate' can take any old-format
-- workspace and move it forward to the current time; it is basically a
-- compatibility test (see also the 'schema_migration' test).
--
-- This means that every time the workspace format is changed, you need to add
-- a new piece to this test, for the new format.  The way you do this is to
-- run this test with the -d option, like so:
--   $ ./testsuite.lua -d workspace_migration
-- this will cause the test to leave behind its temporary files.  You want
--   tester_dir/workspace_migration/current/
-- copy that directory to this directory, named test-<format version>, and
-- update the 'current_workspace_format' variable at the top of this file.

local current_workspace_format = 2

mtn_setup()

--------------------------------------------------------------------------------------------------------------------------------------------
---- Do not touch this code; you'll have to regenerate all the test
---- workspaces if you do!
--------------------------------------------------------------------------------------------------------------------------------------------

addfile("testfile1", "blah blah\n")
addfile("testfile2", "asdfas dfsa\n")
check(mtn("attr", "set", "testfile1", "test:attr", "fooooo"), 0, false, false)
commit("testbranch")
base_rev = base_revision()

check(mtn("checkout", "-r", base_rev, "current"), 0, false, false)
-- make some edits to the files
writefile("current/testfile1", "new stuff\n")
writefile("current/testfile2", "more new stuff\n")
-- and some tree rearrangement stuff too
check(indir("current", mtn("rename", "--execute", "testfile2", "renamed-testfile2")),
      0, false, false)
check(indir("current", mtn("attr", "set", "renamed-testfile2", "test:attr2", "asdf")),
      0, false, false)
check(indir("current", mtn("attr", "drop", "testfile1", "test:attr")),
      0, false, false)
mkdir("current/newdir")
writefile("current/newdir/file3", "twas mimsy and the borogroves\n")
check(indir("current", mtn("add", "newdir", "newdir/file3")), 0, false, false)

-- _MTN/log
writefile("current/_MTN/log", "oh frabjous patch, calloo callay\n")
-- _MTN/monotonerc
writefile("current/_MTN/monotonerc",
          '-- io.stderr:write("warning: bandersnatch insufficiently frumious\\n")\n')

-- _MTN/options

-- we set all the options, by hand, to complete nonsense, because
-- (a) the migration operation is not supposed to need any information
-- from this file, and (b) monotone should not clobber the options
-- file, even if the corresponding command line options are given, when
-- it doesn't understand the bookkeeping format.  we save the nonsense
-- separately from current/_MTN/options to ensure that that, too, isn't
-- getting clobbered.

writefile("nonsense-options",
          'database "/twas/brillig/and/the/slithy/toves.mtn"\n'..
	  '  branch "did.gyre.and.gimble.in.the.wabe"\n'..
	  '     key "all.mimsy.were@the.borogoves"\n'..
	  '  keydir "/and/the/mome/raths/outgrabe"\n')
copy("nonsense-options", "current/_MTN/options")

--------------------------------------------------------------------------------------------------------------------------------------------
---- End untouchable code
--------------------------------------------------------------------------------------------------------------------------------------------

function check_workspace_matches_current(ws)
   check(samefile("nonsense-options", ws.."/_MTN/options"))
   check(indir("current", mtn("automate", "get_revision")), 0, true, false)
   rename("stdout", "current-rev")
   check(indir(ws, mtn("automate", "get_revision")), 0, true, false)
   rename("stdout", ws .. "-current-rev")
   check(samefile("current-rev", ws.."-current-rev"))
   -- and the log file
   check(samefile("current/_MTN/log", ws .. "/_MTN/log"))
   -- we'd like to check that the hook file is being read, but we can't,
   -- because we can't tell monotone to read _MTN/monotonerc without also
   -- telling it to read ~/.monotone/monotonerc, and that would be bad in a
   -- test environment.  So we content ourselves with just checking the file
   -- came through and is in the right place.
   --check(indir(ws, mtn("status")), 0, true, false)
   --check(qgrep("bandersnatch", "stderr"))
   check(samefile("current/_MTN/monotonerc", ws .. "/_MTN/monotonerc"))
end

function check_migrate_from(version)
   L(locheader(),
     "checking migration from workspace format version ", version, "\n")
   local ws = "test-" .. version
   get(ws, ws)
   if (exists(ws .. "/_MTN/format")) then
      check(readfile(ws .. "/_MTN/format") == (version .. "\n"))
   else
      check(1 == version)
   end
   if current_workspace_format ~= version then
      -- monotone notices and refuses to work
      check(indir(ws, mtn("status")), 1, false, true)
      -- and the error message mentions the command they should run
      check(qgrep("migrate_workspace", "stderr"))
   end
   -- use raw_mtn here so it's not getting any help from the command line
   check(indir(ws, raw_mtn("migrate_workspace")), 0, false, false)
   -- now we should be the current version, and things should work
   check(readfile(ws .. "/_MTN/format") == (current_workspace_format .. "\n"))
   check_workspace_matches_current(ws)
   check(indir(ws, mtn("status")), 0, false, false)
end

for i = 1,current_workspace_format do
   check_migrate_from(i)
end