#ifndef TABLES_HPP
#define TABLES_HPP

#include "StorageEngine.hpp"

/**
 * @class Table
 * @brief Abstract base for tables in a miniDBMS.
 *
 * Records are laid out with key fields first, followed by non-key fields
 * (as defined by Relation).  All operations return records by value so
 * that callers never hold references invalidated by container growth or
 * transient caches.
 *
 * Update operations guarantee a *validate-before-mutate* discipline:
 * every new value is checked (field membership, size, domain constraint)
 * before any data is written.  If the key is changed the operation also
 * rejects the new key when it would duplicate an existing record.  On
 * failure the original record is left unchanged.
 */
class Table {
protected:
    shared_ptr<Relation> rel;
public:
    explicit Table(shared_ptr<Relation> rel);
    virtual ~Table() = default;

    /// Insert a validated Record.  Throws if a duplicate key already exists.
    virtual void addRecord(Record record) = 0;

    /// Convenience: construct a Record from raw bytes and insert it.
    virtual void addRecord(string data);

    /**
     * @brief Look up a record by its key prefix.
     * @return The Record if found, or nullopt if no record has that key.
     * @throws invalid_argument if key size does not match the relation's key size.
     */
    virtual optional<Record> getRecord(string_view key) = 0;

    /**
     * @brief Delete the record identified by key.
     * @return The deleted Record, or nullopt if the key was not found.
     * @throws invalid_argument if key size does not match the relation's key size.
     */
    virtual optional<Record> deleteRecord(string_view key) = 0;

    /**
     * @brief Update fields of the record identified by @p key.
     *
     * Semantics:
     *   1. Validate key length upfront.
     *   2. Locate the old record.  Return false if not found.
     *   3. Copy the old record into a candidate and apply every new Value
     *      via Record::setValue (which validates field membership, size, and
     *      domain constraints; throws on failure).
     *   4. If the key was changed, reject the update when the new key already
     *      belongs to a different record (return false).
     *   5. Commit: assign the candidate back (virtual) or call File::updateData
     *      (physical).
     *
     * On any failure before commit the original record remains untouched.
     *
     * @return true on success, false if the key was not found or the new key
     *         duplicates an existing record.
     * @throws invalid_argument if key size is wrong.
     * @throws invalid_argument if a Value fails validation (via setValue).
     */
    virtual bool updateRecordByKey(string_view key, const vector<Value>& newValues) = 0;

    /**
     * @brief Return every record by value.
     *
     * This is deliberately simple — a full heap scan — and serves as the
     * educational boundary for the future executor.
     */
    virtual vector<Record> scan() = 0;

    shared_ptr<Relation> getRelation();
};


/**
 * @class VirtualTable
 * @brief An in-memory table backed by a vector of Records.
 *
 * All data lives in volatile memory and is lost when the object is destroyed.
 * Key uniqueness is enforced at insert and update time.
 */
class VirtualTable : public Table {
    vector<Record> records;
public:
    explicit VirtualTable(shared_ptr<Relation> rel);

    using Table::addRecord;
    void addRecord(Record record) override;
    optional<Record> getRecord(string_view key) override;
    optional<Record> deleteRecord(string_view key) override;
    bool updateRecordByKey(string_view key, const vector<Value>& newValues) override;
    vector<Record> scan() override;
};


/**
 * @class PhysicalTable
 * @brief A persistent table backed by a File (HeapFile).
 *
 * Records are stored in a fixed-width binary heap file.  Operations
 * delegate to the File interface for persistence so that every mutation
 * is immediately visible.
 */
class PhysicalTable : public Table {
    string name;
    FilePtr file;
public:
    PhysicalTable(shared_ptr<Relation> rel, string name, FilePtr file);

    using Table::addRecord;
    void addRecord(Record record) override;
    optional<Record> getRecord(string_view key) override;
    optional<Record> deleteRecord(string_view key) override;
    bool updateRecordByKey(string_view key, const vector<Value>& newValues) override;
    vector<Record> scan() override;

    const string& getName() const;
};

using PhysicalTableRef = reference_wrapper<PhysicalTable>;

#endif // TABLES_HPP
