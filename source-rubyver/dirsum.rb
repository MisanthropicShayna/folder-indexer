require 'digest'

# Extend ruby's File class to include an each_chunk method, used in conjunction with open().
class File
  def each_chunk(chunk_size = 1024*1024)
    yield read(chunk_size) until eof?
  end
end

# By default, the target directory is the current directory.
target_directory = '.'
sha2_length = 256

ARGV.each_with_index do |argument, index|
  next_argument = index + 1 < ARGV.size ? ARGV[index + 1] : nil

  if ['--input', '-i'].include?(argument) && !next_argument.nil?
    (puts "The provided directory doesn't exist."; exit) unless File.directory?(next_argument)
    target_directory = next_argument
  elsif ['--sha', '-s'].include?(argument) && !next_argument.nil?
    (puts "Valid lengths: 256, 384, 512 - not #{next_argument}"; exit) unless ['256', '384', '512'].include?(next_argument)

    sha2_length = next_argument.to_i
  end
end

puts "Scanning for files..  "
directory_files = Dir.glob("#{target_directory}/**/*", File::FNM_DOTMATCH).select { |entry| File.file?(entry) }

puts "#{directory_files.size} Files queued for hashing.."

last_file_path_size = 12
files_hashed = 1

file_hexdigests = directory_files.map do |file_path|
  sha2_digest_object = Digest::SHA2.new(sha2_length)

  print "....#{((files_hashed.to_f / directory_files.size.to_f) * 100.0).round(3)}% --> #{File.basename(file_path)}".ljust(last_file_path_size, ' ')
  print "\r"

  last_file_path_size = file_path.size + 15
  files_hashed += 1

  open(file_path, 'rb') do |file|
    file.each_chunk { |chunk| sha2_digest_object.update(chunk) }
  end

  sha2_digest_object.hexdigest!
end.sort

folder_hexdigest = file_hexdigests.inject(Digest::SHA2.new(sha2_length)) do |sha2_digest_object, hexdigest|
  sha2_digest_object.update(hexdigest)
end.hexdigest

puts "\nHexdigest of  #{file_hexdigests.size} files: #{folder_hexdigest}"