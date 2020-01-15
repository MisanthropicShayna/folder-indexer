require 'digest'
require 'json'

class File
  def each_chunk(chunk_size = 1024*1024)
    yield read(chunk_size) until eof?
  end
end

def CompareIndexes(old_index_hash, new_index_hash)
  if old_index_hash['directory_hexdigest'] == new_index_hash['directory_hexdigest']
    puts "\nMATCHING SHA512 DIRECTORY HASHES, FURTHER COMPARISONS NOT NEEDED."
    puts '-' * 132
    puts "OLD #{old_index_hash['directory_hexdigest']}"
    puts "NEW #{new_index_hash['directory_hexdigest']}"
    puts '-' * 132

    exit
  else
    puts "\nDISCREPANCY IN SHA512 DIRECTORY HASHES, FURTHER COMPARISONS IN PROGRESS.."
    puts '-' * 132
    puts "OLD #{old_index_hash['directory_hexdigest']}"
    puts "NEW #{new_index_hash['directory_hexdigest']}"
    puts '-' * 132
    puts "\n"
  end

  old_hexdigest_table = old_index_hash['file_hexdigest_table']
  new_hexdigest_table = new_index_hash['file_hexdigest_table']

  missing_files = old_hexdigest_table.select { |file, hexdigest| !new_hexdigest_table.include?(file) }
  new_files = new_hexdigest_table.select { |file, hexdigest| !old_hexdigest_table.include?(file) }

  modified_files = new_hexdigest_table.select do |file, hexdigest|
    old_hexdigest_table.include?(file) && old_hexdigest_table[file] != new_hexdigest_table[file]
  end.map { |file, hexdigest| [file, {'old' => old_hexdigest_table[file], 'new' => new_hexdigest_table[file]}] }

  new_files.each do |file, hexdigest|
    puts "#{'NEW FILE'.ljust(15, '.')} #{file}"
    puts "- (#{hexdigest})\n\n"
  end

  puts "#{'-'*132}\n\n" if new_files.size > 0

  missing_files.each do |file, hexdigest|
    puts "#{'FILE ABSENT'.ljust(15, '.')} #{file}"
    puts "- (#{hexdigest})\n\n"
  end
  
  puts "#{'-'*132}\n\n" if missing_files.size > 0

  modified_files.each do |file, hexdigests|
    puts "#{'FILE MODIFIED'.ljust(15, '.')} #{file}"
    puts "- OLD: (#{hexdigests['old']})"
    puts "- NEW: (#{hexdigests['new']})\n\n"
  end

  puts "#{'-'*132}\n\n" if modified_files.size > 0
end

def CompareIndexFiles(old_index_file, new_index_file)
  # If either the old, or new index files provided aren't valid files, log and exit.
  (puts "The provided old index file '#{old_index_file}' is not a valid file."; exit) if !File.file?(old_index_file)
  (puts "The provided new index file '#{new_index_file}' is not a valid file."; exit) if !File.file?(new_index_file)

  old_index = JSON.load open(old_index_file, 'r') { |file| file.read }
  new_index = JSON.load open(new_index_file, 'r') { |file| file.read }  

  CompareIndexes(old_index, new_index)
end

def ReevaluateIndex(index_file)
  # Exit if the index file provided isn't valid.
  (puts "The index file '#{index_file}' does not exist, and cannot be re-evaluated.";exit) if !File.file?(index_file)

  old_directory_index = JSON.load(open(index_file, 'r') { |file| file.read })
  new_directory_index = JSON.load(BuildIndex(old_directory_index['directory'], nil))

  CompareIndexes(old_directory_index, new_directory_index)
end 

def BuildIndex(directory, out_index_file)
  # Exit if the directory provided isn't valid.
  (puts "The provided directory '#{directory}' is not a valid directory."; exit) if !File.directory?(directory)

  directory_files = Dir.glob("#{directory}/**/*", File::FNM_DOTMATCH).select { |entry| File.file? entry }

  files_hexdigest_table = (directory_files.map do |file|
    sha256_digest_object = Digest::SHA2.new(256)

    open(file, 'rb').each_chunk do |chunk|
      sha256_digest_object.update(chunk)
    end

    [file, sha256_digest_object.hexdigest!]
  end).to_h

  directory_hexdigest = (files_hexdigest_table.map{|file, hexdigest| hexdigest}.sort.inject(Digest::SHA2.new(512)) do |sha512_digest_object, kvpair|
    sha512_digest_object.update(kvpair[1])
  end).hexdigest

  json_payload = JSON.pretty_generate({
    'directory' => directory,
    'directory_hexdigest' => directory_hexdigest,
    'file_hexdigest_table' => files_hexdigest_table
  })

  if out_index_file == STDOUT
    puts json_payload
  elsif out_index_file == nil
    return json_payload
  else
    begin
      open(out_index_file, 'w+') { |file| file.write(json_payload) }
    rescue IOError => io_error
      puts "Cannot write directory index to file '#{out_index_file}' due to an IOError."
      puts "Exception message: #{io_error.message}"
    end
  end
end

# Whether or not a valid mode has been specified.
# If this remains false by the end of argument parsing, the program will exit.
mode_has_been_specified = false

# Parse the command line arguments to determine the mode, and parse
# arguments specific to that mode before calling the function
# ascociated with that mode.

ARGV.each_with_index do |argument, index|
  next_argument = index + 1 < ARGV.size ? ARGV[index + 1] : nil

  if ['--mode', '-m'].include?(argument) && !next_argument.nil?
    # This table maps operation modes to argument parsing lambdas.
    # This ensures that only the arguments required are ever stored into variables,
    # and to avoid poluting the global scope. The arguments are then passed to the 
    # ascociated function respectively.
    mode_handler_table = {
      "build" => lambda {
        out_index_file = STDOUT
        directory = nil

        ARGV.each_with_index do |mode_argument, mode_argument_index|
          mode_next_argument = mode_argument_index + 1 < ARGV.size ? ARGV[mode_argument_index + 1] : nil

          if ['--input', '-i'].include?(mode_argument) && !mode_next_argument.nil?
            directory = mode_next_argument
          elsif ['--output', '-o'].include?(mode_argument) && !mode_next_argument.nil?
            out_index_file = mode_next_argument
          end
        end

        # If a directory hasn't been provided, BuildIndex cannot be called, and the program must exit.
        (puts "Please provide the target directory for the index via --input(-i)";exit) if directory.nil?
        
        # Pass on the parsed arguments to the BuildIndex function.
        BuildIndex(directory, out_index_file)
      },

      "reval" => lambda  {
        input_index_file = nil

        ARGV.each_with_index do |mode_argument, mode_argument_index|
          mode_next_argument = mode_argument_index + 1 < ARGV.size ? ARGV[mode_argument_index + 1] : nil
          
          if['--input', '-i'].include?(mode_argument) && !mode_next_argument.nil?
            input_index_file = mode_next_argument
          end  
        end

        # If no index file was provided to re-evaluate, ReevaluateIndex cannot be called, and the program must exit.
        (puts "Please provide an index file to re-evaluate via --input(-i)";exit) if input_index_file.nil?

        ReevaluateIndex(input_index_file)
      },
      
      "compare" => lambda {
        new_index_file = nil
        old_index_file = nil
        
        ARGV.each_with_index do |mode_argument, mode_argument_index|
          mode_next_argument = mode_argument_index + 1 < ARGV.size ? ARGV[mode_argument_index + 1] : nil
          
          if ['--new-index', '-ni'].include?(mode_argument) && !mode_next_argument.nil?
            new_index_file = mode_next_argument
          elsif ['--old-index', '-oi'].include?(mode_argument) && !mode_next_argument.nil?
            old_index_file = mode_next_argument
          end
        end
        
        # If either the new, or old index wasn't provided, CompareIndexFiles cannot be called, and the program must exit.
        (puts "Please provide the new index file via --new-index(-ni) to make the comparison.";exit) if new_index_file.nil?
        (puts "Please provide the old index file via --old-index(-oi) to make the comparison.";exit) if old_index_file.nil?

        CompareIndexFiles(old_index_file, new_index_file)
      }
    }

    if mode_handler_table.include?(next_argument)
      # If the provided mode is valid, call the lambda in the mode handler table.

      mode_has_been_specified = true
      mode_handler_table[next_argument].call
      
      # Break to avoid looping through more arguments than necessary.
      break 
    else 
      # Otherwise, log the error and exit.

      puts "The mode '#{next_argument}' is not a valid mode."
      exit
    end

  end
end

# Ensure the program exits in case a mode hasn't been specified.
if !mode_has_been_specified
    puts 'Please provide a mode through --mode(-m).'
    exit
end