#ifndef FILE_HPP
#define FILE_HPP

#include <string>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

using namespace std;

/**
 * @class File
 * @brief Represents a file in the miniDBMS system.
 * 
 * The File class provides functionality to interact with a file at a low level in the miniDBMS system.
 * It provides methods to scan, insert, lookup, update, and delete fixed-width records.
 * 
 */
class File {
    string name;
protected:
    fstream file;
public:
    File(string fileName);
    virtual ~File() = default;

    /**
     * @return file name as a reference
     */
    const string& filename() const;
    /**
     * @brief flush the main stream of the file
     */
    void flush();


    /**
     * @brief Scan all records in the file.
     *
     * @return A vector of all records as raw strings.
     */
    virtual vector<string> scan() = 0;

    /**
     * @brief Insert a single fixed-width record.
     *
     * @param record The record data; must be exactly recordSize bytes.
     */
    virtual void insert(string_view record) = 0;

    /**
     * @brief Look up a record by key prefix.
     *
     * @param key The key to search for; must be exactly keySize bytes.
     * @return The record if found, nullopt otherwise.
     */
    virtual optional<string> getData(string_view key) = 0;

    /**
     * @brief Overwrite the record identified by key with newRecord.
     *
     * The caller must ensure that newRecord has the correct size and that
     * a record with the given key exists.
     *
     * @param key The key of the record to update; must be exactly keySize bytes.
     * @param newRecord The replacement record data; must be exactly recordSize bytes.
     */
    virtual void updateData(string_view key, string_view newRecord) = 0;

    /**
     * @brief Delete the record identified by key.
     *
     * The deleted record is returned. The last record is swapped into the
     * vacated slot and the physical file is truncated immediately.
     *
     * @param key The key of the record to delete; must be exactly keySize bytes.
     * @return The deleted record, or nullopt if the key was not found.
     */
    virtual optional<string> deleteData(string_view key) = 0;

};

using FilePtr = unique_ptr<File>;

#endif // FILE_HPP
