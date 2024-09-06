#ifndef FILE_HPP
#define FILE_HPP

#include <string>
#include <fstream>
#include <optional>

using namespace std;

/**
 * @class File
 * @brief Represents a file in the miniDBMS system.
 * 
 * The File class provides functionality to interact with a file at a low level in the miniDBMS system.
 * It allows flushing and syncing of the file, as well as iterating over the records in the file.
 * It also provides methods to insert, delete, and retrieve data from the file.
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
     * @brief sync the main stream of the file
     */
    void sync();

    /**
     * @brief Returns an iterator of records without ordering.
     *
     * @return An iterator of records.
     */
    virtual iterator<input_iterator_tag,string> begin() = 0;

    /**
     * @return Mark the end of a iterator in the file.
     */
    virtual iterator<input_iterator_tag,string> end() = 0;

    /**
     * @brief push data on the file
     */
    virtual void pushData(string_view data) = 0;
    /**
     * @brief delete data from the file
     */
    virtual optional<string> deleteData(string_view key) = 0;
    /**
     * @brief search data starting with a key
     * 
     * @return nullptr if the record don't exists, a string otherwise
     */
    virtual optional<string> getData(string_view key) = 0;

};

using SharedFile = shared_ptr<File>;

#endif // FILE_HPP