# Folder Indexer
This Ruby script combo allows for two things: folder indexing, and folder checksumming.

## findexer.rb
The primary script. This script can generate a folder "index" (.json) containing a dictionary of every single file in a directory, and its corresponding hexdigest (sha256), as well as a hexdigest of the root folder (calculated by summing the hexdigests of every file, in alphabetical order). This index can be compared with another index, or re-evaluated (essentially generating a new index of the same directory in memory and comparing it without ever storing it in a file).

### Command Line Arguments
- `--mode(-m) [build | compare | reval]` Specifies the operation mode. The rest of the arguments depend on what mode has been set.

- `--input(-i)` Applies to `build` and `reval` - specifies the target directory in the case of `build`. Specifies an index file in case of `reval`.

- `--output(-o)` Applies to `build` - specifies where to write the index. If left unspecified, stdout is used instead.

- `--old-index(-oi)` Applies to `compare` - specifies the "old" index in the comparison. Affects whether or not a file is treated as new/absent.

- `--new-index(-ni)` Applies to `compare` - specifies the "new" index in the comparison. Affects whether or not a file is treated as new/absent.

### Example Usage
```
λ findexer --mode build --input . --output ../index.json
λ echo "Hello World!" > hello_world.txt
λ echo "The magic number is: 42" > magic_number.txt
λ findexer --mode reval --input ../index.json

DISCREPANCY IN SHA512 DIRECTORY HASHES, FURTHER COMPARISONS IN PROGRESS..
------------------------------------------------------------------------------------------------------------------------------------
OLD cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e
NEW 2b5801d08603c0d5f4db58b71dc5ff22e1e3d3b88ac43dcc4468d1cbbaf3c4631c066a848818c4ed52fb329ec8a9ea77aa470d397040f2d938d1a9c8da6fa96c
------------------------------------------------------------------------------------------------------------------------------------

NEW FILE....... ./hello_world.txt
- (de02dc0b26d5826c0ccd4e4c60178713d645c2082d1bce10b3501233df20d8cf)

NEW FILE....... ./magic_number.txt
- (1d72c70935282b87d002e2dfff6f771a0ab46bbacd2c2ab8ca81bb2b56617465)

------------------------------------------------------------------------------------------------------------------------------------

λ findexer --mode build --input . --output ../index_new.json
λ echo "The magic number is: 420" > magic_number.txt
λ findexer --mode build --input . --output ../index_new_2.json
λ findexer --mode compare --old-index ../index_new.json --new-index ../index_new_2.json

DISCREPANCY IN SHA512 DIRECTORY HASHES, FURTHER COMPARISONS IN PROGRESS..
------------------------------------------------------------------------------------------------------------------------------------
OLD 2b5801d08603c0d5f4db58b71dc5ff22e1e3d3b88ac43dcc4468d1cbbaf3c4631c066a848818c4ed52fb329ec8a9ea77aa470d397040f2d938d1a9c8da6fa96c
NEW 52cf3c915bb75d5c2b439d846b65e3979c0f77f18340be14bc88c1731c1dc3ebeb86d9650a15c56ccaeb8ecd387ad49da4932297c7e7b747d285311d65a6dbe8
------------------------------------------------------------------------------------------------------------------------------------

FILE MODIFIED.. ./magic_number.txt
- OLD: (1d72c70935282b87d002e2dfff6f771a0ab46bbacd2c2ab8ca81bb2b56617465)
- NEW: (14c5ce23c0bf418946ae68c5f9871d9e129a511632111abb46750eeb12df9c3e)

------------------------------------------------------------------------------------------------------------------------------------

λ rm *.txt
λ findexer --mode reval --input ../index_new.json

DISCREPANCY IN SHA512 DIRECTORY HASHES, FURTHER COMPARISONS IN PROGRESS..
------------------------------------------------------------------------------------------------------------------------------------
OLD 2b5801d08603c0d5f4db58b71dc5ff22e1e3d3b88ac43dcc4468d1cbbaf3c4631c066a848818c4ed52fb329ec8a9ea77aa470d397040f2d938d1a9c8da6fa96c
NEW cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e
------------------------------------------------------------------------------------------------------------------------------------

FILE ABSENT.... ./hello_world.txt
- (de02dc0b26d5826c0ccd4e4c60178713d645c2082d1bce10b3501233df20d8cf)

FILE ABSENT.... ./magic_number.txt
- (1d72c70935282b87d002e2dfff6f771a0ab46bbacd2c2ab8ca81bb2b56617465)

------------------------------------------------------------------------------------------------------------------------------------
```

## dirsum.rb
The secondary script. Can be used to perform a checksum of an entire directory by hashing every file in the directory and sorting the hexdigests in alphabetical order before feeding them into the digest object's update function one by one.

### Command Line Arguments
- `--input(-i)` The path to the directory that should be checksummed.
- `--sha(-s)` The sha2 mode, can be either 256, 384, or 512.

### Example Usage
```
λ dirsum --input ../../lspm -sha 384                                                                                                                                        
Scanning for files..
463 Files queued for hashing..
....100.0% --> main_dlg.ui
Hexdigest of  463 files: 73e0235375c98fc41db3d7804546ff626269b63c260476a00cf45b14fd50e00f
```


