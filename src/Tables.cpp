#include "Tables.hpp"

// Table

Table::Table(shared_ptr<Relation> rel)
: rel(rel) {}

shared_ptr<Relation> Table::getRelation() { return rel; }

// virtual Table

void VirtualTable::addRecord(Record record) {
    if(getRecord(record.getKeyData()).has_value())
        throw invalid_argument("Broken Key Constraint");
    this->records.push_back(record);
}

void VirtualTable::addRecord(string data) {
    Record newRecord(rel, data);

    addRecord(newRecord);
}

optional<ConstRecordRef> VirtualTable::getRecord(string_view key) {
    if(rel.get()->getKeySize() != key.length()) //TODO: this does not check for domain costraint
        throw invalid_argument("The key is not valid");

    for(const Record& record : records) {
        if(record.getKeyData() == key)
            return record;
    }
    return {};
}

optional<Record> VirtualTable::deleteRecord(string_view key) {
    if(rel.get()->getKeySize() != key.length()) //TODO: this does not check for domain costraint
        throw invalid_argument("The key is not valid");

    for(auto it = records.begin(); it != records.end(); ++it) {
        if((*it).getKeyData() == key) {
            Record result = *it;
            records.erase(it);
            return result;
        }
    }
    return {};
}

bool VirtualTable::updateRecordByKey(string_view key, const vector<Value>& newValues) {
    for(Record& record : records) {
        if(record.getKeyData() == key) {
            for(const Value& val : newValues)
                record.setValue(val); //TODO: verificare il record prima di settare il nuovo valore
            return true;
        }
    }
    return false;
}

// PhysicalTable

PhysicalTable::PhysicalTable(shared_ptr<Relation> rel, string name, FilePtr file)
: Table(rel), name(name), file(move(file)) {}

void PhysicalTable::addRecord(Record record) {
    auto f = file.get();

    if(f->getData(record.getKeyData()).has_value())
        throw invalid_argument("Primary Key constraint violated");
    f->pushData(record.getData());

}

void PhysicalTable::addRecord(string data) {
    Record newRecord(rel,data);

    addRecord(newRecord);
}

optional<ConstRecordRef> PhysicalTable::getRecord(string_view key) {
    auto f = file.get();
    auto raw_record = f->getData(key);
    if(raw_record.has_value()) {
        Record record(rel, raw_record.value());
        volatileRecords.push_back(record);
        return volatileRecords.back();
    }
    return {};
}

optional<Record> PhysicalTable::deleteRecord(string_view key) {
    auto f = file.get();
    auto data = f->deleteData(key);
    if(data.has_value())
        return Record(rel, data.value());
    return {};
}

bool PhysicalTable::updateRecordByKey(string_view key, const vector<Value>& newValues) {
    //TODO: modificare il record del file senza cancellarlo e reinserirlo
    auto f = file.get();
    auto raw_record = f->deleteData(key);
    if(!raw_record.has_value())
        return false;
    Record newRecord(rel, raw_record.value());
    for(const Value& val : newValues)
        newRecord.setValue(val); //TODO: verificare il record prima di settare il nuovo valore
    f->pushData(newRecord.getData());
    return true;
}

const string& PhysicalTable::getName() const { return name; }

void PhysicalTable::clear() { volatileRecords.clear(); }