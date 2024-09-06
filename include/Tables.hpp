#ifndef TABLES_HPP
#define TABLES_HPP

#include "StorageEngine.hpp"

/**
 * @class Table
 * @brief Represents a table in a miniDBMS.
 * 
 * The Table class provides functionality to manipulate and interact with a table in the miniDBMS.
 * It contains methods to add records, search for records, delete records, and update records.
 * 
 * @note This class is meant to be inherited from and should not be instantiated directly.
 */
class Table {
protected:
    shared_ptr<Relation> rel;
public:

    Table(shared_ptr<Relation> rel);

    /**
     * @brief Adds a record to the table.
     *
     * @param record The record to be added.
     */
    virtual void addRecord(Record record);

    /**
     * @brief Adds a record to the table.
     *
     * @param data The raw data to be added.
     */
    virtual void addRecord(string data);

    //TODO: virtual VirtualTable search(QueryPlan plan) const;

    //TODO: virtual JoinedTable join(Table& rightTable);

    /**
     * @brief get a Reference to a Record
     * 
     * @note this function is not constant to allow flexibility to the various implementations of the Table class
     * 
     * @param key raw data that rappresent a key
     * @return nullptr if the record don't exist or a constant reference to the Record
     * @throw invalid_argument if the key is not valid
     */
    virtual optional<ConstRecordRef> getRecord(string_view key);

    /**
     * @brief delete a Record
     * 
     * @param key vector of values of a key
     * @return nullptr if the record don't exist or the Record
     * @throw invalid_argument if the key is not valid
     */
    virtual optional<Record> deleteRecord(string_view key);

    /**
     * @brief update the values of a Record.
     * 
     * @param key the key of the record you want to edit.
     * @param newValues vector of values to be replaced.
     * 
     * @return false if the key of the newRecord don't exist, true otherwise.
     */
    virtual bool updateRecordByKey(string_view key, const vector<Value>& newValues);

    /**
     * @brief getter for rel
     */
    shared_ptr<Relation> getRelation();

};


/**
 * @class VirtualTable
 * @brief Represents a virtual table in a miniDBMS.
 * 
 * The VirtualTable class provides functionality to manipulate and interact with a table,but the records are stored only
 * in volatile memory.
 * It contains methods to add records, search for records, delete records, and update records.
 * 
 */
class VirtualTable: public Table {

    vector<Record> records;

public:

    void addRecord(Record record) override;

    void addRecord(string data) override;

    optional<ConstRecordRef> getRecord(string_view key) override;

    optional<Record> deleteRecord(string_view key) override;

    bool updateRecordByKey(string_view key, const vector<Value>& newValues) override;

};

class PhysicalTable: public Table {
    string name;
    // records salvati nella RAM e non sul disco rigito
    vector<Record> volatileRecords;
    SharedFile file;
public:
    PhysicalTable(shared_ptr<Relation> rel, string name, SharedFile file);

    void addRecord(Record record) override;

    void addRecord(string data) override;

    optional<ConstRecordRef> getRecord(string_view key) override;

    optional<Record> deleteRecord(string_view key) override;

    bool updateRecordByKey(string_view key, const vector<Value>& newValues) override;

    const string& getName() const;

    void clear();
};

using PhysicalTableRef = reference_wrapper<PhysicalTable>;

#endif // TABLES_HPP