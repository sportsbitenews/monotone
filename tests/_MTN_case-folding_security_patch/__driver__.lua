
mtn_setup()

-- The patch for this security issue is to treat all case-folded
-- versions of _MTN as being bookkeeping files (and thus illegal
-- file_paths).  Make sure it's working.

names = {"_mtn", "_mtN", "_mTn", "_Mtn", "_MTn", "_MtN", "_mTN", "_MTN"}

-- bookkeeping files are an error for add
for _,i in pairs(names) do if not exists(i) then writefile(i) end end
for _,i in pairs(names) do
  check(mtn("add", i), 1, false, false)
end
check(mtn("ls", "known"), 0)
for _,i in pairs(names) do remove(i) end

-- run setup again, because we've removed our bookkeeping dir.
check(mtn("--branch=testbranch", "setup", "."))

-- files in bookkeeping dirs are also ignored by add
-- (mkdir -p used because the directories already exist on case-folding FSes)
for _,i in pairs(names) do
  if not exists(i) then mkdir(i) end
  writefile(i.."/foo", "")
end
for _,i in pairs(names) do
  check(mtn("add", i), 1, false, false)
end
check(mtn("ls", "known"), 0)
for _,i in pairs(names) do remove(i) end

-- assert trips if we have a db that already has a file with this sort
-- of name in it.
for _,i in {"files", "dirs"} do
  get(i..".db.dumped", "stdin")
  check(mtn("db", "load", "-d", i..".mtn"), 0, false, false, true)
  check(mtn("db", "migrate", "-d", i..".mtn"), 0, false, false)
  check(mtn("-d", i..".mtn", "co", "-b", "testbranch", i.."-co-dir"), 3, false, false)
end