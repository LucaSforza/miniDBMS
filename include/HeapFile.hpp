#ifndef HEAPFILE_HPP
#define HEAPFILE_HPP

#include "File.hpp"

/**
 * @class HeapFile
 * @brief Store raw records as a heap. The file has no particular sort order or structure.
 */
class HeapFile: public File {

size_t keySize;
size_t recordSize;
long endFilePosition;

public:
    HeapFile(string fileName, size_t keySize, size_t recordSize);

    ~HeapFile() override;

    iterator<input_iterator_tag,string> begin() override;
    iterator<input_iterator_tag,string> end() override;
    void pushData(string_view data) override;
    optional<string> deleteData(string_view key) override;
    optional<string> getData(string_view key) override;

private:

    /**
     * @brief search the position on the file of a record with a selected key
     * @param key the key of the record to search
     * 
     * @return -1 if the record don't exist or the starting position of the record
     */
    long searchPosition(string_view key);

    /**
     * @brief get the data of the last record on the file
     * 
     * @return nullptr if the file is with no records, raw data otherwise
     */
    optional<string> getLastRecord();

    /**
     * @brief delete the last record of the file, the record on the file 
     * is not actually deleted, but the position of the end of the file is reduced
     */
    void removeLastRecord();

    /**
     * @brief truncate file on the filesystem where the end pointer points
     */
    void truncateFile();

};

#endif // HEAPFILE_HPP