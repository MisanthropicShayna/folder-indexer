#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include <openssl/sha.h>

#include <json.hpp>
using Json = nlohmann::json;

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <map>

#include <thread>
#include <atomic>
#include <mutex>

#include <Windows.h>
#define WIN32_MEAN_AND_LEAN


std::string Hexlify(const std::vector<uint8_t>& data) {
    std::stringstream string_stream;
    for(const uint8_t& byte : data) {
        string_stream << std::hex << static_cast<int>(byte);
    }
    return string_stream.str();
}

Json BuildFolderIndex(const std::string& folder_path, uint32_t thread_count = 10, size_t chunk_size = 10000000) {
    std::vector<std::string> queued_files;
    std::mutex queued_files_mutex;

    std::map<std::string, std::string> file_index;
    std::mutex file_index_mutex;

    std::cout << "Queueing files @ " << folder_path << std::endl;

    for(const auto& entry : fs::recursive_directory_iterator(folder_path)) {
        if(fs::is_directory(entry)) continue;
        const std::string& entry_path = entry.path().string();
        queued_files.emplace_back(entry_path);
    }

    size_t total_file_count = queued_files.size();

    std::cout << queued_files.size() << " Files have been queued for hashing." << std::endl;
    std::cout << "Spawning " << thread_count << " threads with a chunk size of " << chunk_size << " bytes." << std::endl;

    std::vector<std::thread> digestor_threads;

    for(int i=0; i<thread_count; ++i) {
        digestor_threads.emplace_back(std::thread([&queued_files, &queued_files_mutex, &file_index, &file_index_mutex, chunk_size]() -> void
            {
                bool files_remaining = true;

                for(;files_remaining;) {
                    std::string next_file;

                    {
                        std::lock_guard<std::mutex> lock_guard(queued_files_mutex);

                        if(queued_files.size() == 0) {
                            files_remaining = false;
                            break;
                        }

                        next_file = queued_files.back();
                        queued_files.pop_back();
                    }

                    std::ifstream ifile_stream(next_file, std::ios::binary);

                    if(!ifile_stream.good()) {
                        std::lock_guard<std::mutex> lock_guard(file_index_mutex);
                        file_index[next_file] = "BAD_STREAM_SKIPPED";
                        continue;
                    }

                    std::vector<uint8_t> sha256_digest(SHA256_DIGEST_LENGTH);

                    SHA256_CTX ossl_sha256;
                    SHA256_Init(&ossl_sha256);

                    std::vector<char> digest_buffer(chunk_size);

                    for(;!ifile_stream.eof();) {
                        ifile_stream.read(digest_buffer.data(), chunk_size);
                        uint64_t read_count = ifile_stream.gcount();
                        SHA256_Update(&ossl_sha256, digest_buffer.data(), read_count);
                    }

                    SHA256_Final(sha256_digest.data(), &ossl_sha256);
                    std::string digest_string = Hexlify(sha256_digest);

                    {
                        std::lock_guard<std::mutex> lock_guard(file_index_mutex);
                        file_index[next_file] = digest_string;
                    }
                }
            }));
    }

    std::cout << std::endl;
    std::cout << std::string(30, '-') << std::endl;

    for(;;Sleep(500)) {
        size_t files_left = 0;

        {
            std::lock_guard<std::mutex> lock_guard(queued_files_mutex);
            files_left = queued_files.size();
        }

        if(files_left == 0) break;

        size_t files_hashed = total_file_count - files_left;
        std::cout << "Files remaining -> " << files_left << " (" << ((double)files_hashed / (double)total_file_count) * 100.0 << "%)" << std::string(30, ' ') << '\r';
    }

    std::cout << std::endl;
    std::cout << std::string(30, '-') << std::endl;

    for(auto& thread : digestor_threads) {
        thread.join();
    }

    std::cout << "All thread have been joined." << std::endl << std::endl;

    Json folder_index {{"index", file_index}, {"folder", folder_path}};

    return folder_index;
}

int CompareIndexes(const Json& old_index_json, const Json& new_index_json) {
    const auto old_index = old_index_json.at("index").get<std::map<std::string, std::string>>();
    const auto new_index = new_index_json.at("index").get<std::map<std::string, std::string>>();

    std::vector<std::string> new_files, deleted_files;
    std::map<std::string, std::pair<std::string, std::string>> changed_files;

    for(const std::pair<std::string, std::string>& pair : new_index) {
        const std::string& file_name = pair.first;
        const std::string& file_hash = pair.second;

        // File Modified
        if(old_index.count(file_name) && old_index.at(file_name) != file_hash) {
            const std::string& old_file_hash = old_index.at(file_name);
            changed_files[file_name] = std::make_pair(old_file_hash, file_hash);
        }

        // New File
        else if(!old_index.count(file_name)) {
            new_files.emplace_back(file_name);
        }
    }

    for(const std::pair<std::string, std::string> pair : old_index) {
        const std::string& file_name = pair.first;
        const std::string& file_hash = pair.second;

        // File Deleted
        if(!new_index.count(pair.first)) {
            deleted_files.emplace_back(pair.first);
        }
    }

    for(auto& changed_file : changed_files) {
        const std::string& file_name = changed_file.first;
        const std::pair<std::string, std::string>& hashes = changed_file.second;

        std::cout << "!= CHANGED FILE | " << file_name << std::endl;
        std::cout << "    OLD -> " << hashes.first << std::endl;
        std::cout << "    NEW -> " << hashes.second << std::endl;
        std::cout << std::endl;
    }

    if(!changed_files.size()) {
        std::cout << "== NO CHANGED FILES" << std::endl;
    }

    std::cout << std::endl;

    for(auto& new_file : new_files) {
        std::cout << "+ NEW FILE      | " << new_file << std::endl;
    }

    if(!new_files.size()) {
        std::cout << "== NO NEW FILES" << std::endl;
    }

    std::cout << std::endl;

    for(auto& deleted_file : deleted_files) {
        std::cout << "- DELETED FILE  | " << deleted_file << std::endl;
    }

    if(!deleted_files.size()) {
        std::cout << "== NO DELETED FILES" << std::endl;
    }

    std::cout << std::endl;

    return 0;
}

int CompareIndexes(const std::string& old_index_path, const std::string& new_index_path) {
    std::ifstream if_old(old_index_path, std::ios::binary);

    if(!if_old.good()) {
        std::cout << "Could not open stream to old index file @ " << old_index_path << std::endl;
        return 1;
    }
    
    std::ifstream if_new(new_index_path, std::ios::binary);

    if(!if_new.good()) {
        std::cout << "Could not open stream to new index file @ " << new_index_path << std::endl;
        return 1;
    }

    std::string old_index_str((std::istreambuf_iterator<char>(if_old)), (std::istreambuf_iterator<char>()));
    std::string new_index_str((std::istreambuf_iterator<char>(if_new)), (std::istreambuf_iterator<char>()));

    const Json old_index_json = Json::parse(old_index_str);
    const Json new_index_json = Json::parse(new_index_str);

    return CompareIndexes(old_index_json, new_index_json);
}

int ReevaluateIndex(const std::string& index_file, uint32_t thread_count = 10, size_t chunk_size = 10000000) {
    std::ifstream ifile_stream(index_file, std::ios::binary);

    if(!ifile_stream.good()) {
        std::cerr << "Couldn't open input stream to index file @ " << index_file << std::endl;
        return 1;
    }

    std::string old_index_file_str((std::istreambuf_iterator<char>(ifile_stream)), (std::istreambuf_iterator<char>()));

    Json old_index_json = Json::parse(old_index_file_str);
    const std::string& old_index_dir = old_index_json.at("folder").get<std::string>();
    Json new_index_json = BuildFolderIndex(old_index_dir, thread_count, chunk_size);

    return CompareIndexes(old_index_json, new_index_json);
}

void PrintHelpText() {
    std::cout
        << "This program has one of three operating modes: build, compare, re-evaluate.\n"
        << "The desired mode is specified through the --mode (-m) argument.\n"
        << std::endl
        << "Build Mode (-m build):\n"
        << "    Builds a directory index .json file. This index will contain a tree of the\n"
        << "    files within the folder as well as its subfolders, mapped to their hash.\n"
        << "    The input directory is specified with --build-input (-bld-i).\n"
        << "    The JSON output file is specified with --build-output (-bld-o).\n"
        << std::endl
        << "Compare Mode (-m compare):\n"
        << "    Compares two existing JSON index files, and displays a full list of discrepencies.\n"
        << "    The old file used in the comparison is specified with --compare-old (-cmp-o).\n"
        << "    The new file used in the comparison is specified with --compare-new (-cmp-n).\n"
        << std::endl
        << "Re-evaluate Mode (-m reval):\n"
        << "    Re-evaluates an existing JSON index file, and displays all discrependcies between\n"
        << "    the index, and the current state of the folder at scan-time.\n"
        << "    The input JSON index file is specified with --reval-input (-rev-i)\n"
        << std::endl
        << "Performance Settings:\n"
        << "    This program uses multithreading to process multiple files at once.\n"
        << "    You may change the number of threads using --config-threadcount (-cfg-t). Default: 10\n"
        << "    You may also change how much of each file should get loaded into memory by each thread\n"
        << "    using --config-chunksize (-cfg-c). Default: 10,000,000 (10 MB).\n"
        << std::endl;
}

int main(int argc, char* argv[]) {
    std::string mode_string;

    std::string build_mode_inputdir;
    std::string build_mode_outputjson;

    std::string compare_mode_oldfile;
    std::string compare_mode_newfile;

    std::string reval_mode_inputjson;

    uint32_t index_builder_threadcount = 10; // 10 File Processing Threads.
    size_t index_builder_chunksize = 10000000; // 10 MB Chunk Size Per File

    for(int i=0; i<argc; ++i) {
        const char* argument = argv[i];
        const char* next_argument = i + 1 < argc ? argv[i + 1] : nullptr;

        if(!_stricmp(argument, "--help") || !_stricmp(argument, "-h")) {
            PrintHelpText();
            return 0;
        }

        if(next_argument != nullptr) {
            if(!_stricmp(argument, "--mode") || !_stricmp(argument, "-m")) {
                mode_string = next_argument;
            }

            else if(!_stricmp(argument, "--build-input") || !_stricmp(argument, "-bld-i")) {
                build_mode_inputdir = next_argument;
            }

            else if(!_stricmp(argument, "--build-output") || !_stricmp(argument, "-bld-o")) {
                build_mode_outputjson = next_argument;
            }

            else if(!_stricmp(argument, "--compare-old") || !_stricmp(argument, "-cmp-o")) {
                compare_mode_oldfile = next_argument;
            }

            else if(!_stricmp(argument, "--compare-new") || !_stricmp(argument, "-cmp-n")) {
                compare_mode_newfile = next_argument;
            }

            else if(!_stricmp(argument, "--reval-input") || !_stricmp(argument, "-rev-i")) {
                reval_mode_inputjson = next_argument;
            }

            else if(!_stricmp(argument, "--config-threadcount") || !_stricmp(argument, "-cfg-t")) {
                index_builder_threadcount = std::atoi(next_argument);
            }

            else if(!_stricmp(argument, "--config-chunksize") || !_stricmp(argument, "-cfg-c")) {
                index_builder_chunksize = std::atoi(next_argument);
            }
        }
    }

    if(mode_string == "build") {
        std::cout << "Build mode.." << std::endl;

        Json file_index = BuildFolderIndex(build_mode_inputdir);
        std::ofstream ofile_stream(build_mode_outputjson, std::ios::binary);

        if(ofile_stream.good()) {
            std::string json_output_string = file_index.dump(4);
            ofile_stream.write(json_output_string.data(), json_output_string.size());
        } else {
            std::cerr << "Couldn't open a stream to the output file." << std::endl;
            return 1;
        }
    }

    else if(mode_string == "compare") {
        CompareIndexes(compare_mode_oldfile, compare_mode_newfile);
    }

    else if(mode_string == "reval") {
        ReevaluateIndex(reval_mode_inputjson);
    }

    return 0;
}
