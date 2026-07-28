#include "StorageEngine.hpp"

// -----------------------------------------------------------------------
// Table (abstract base)
// -----------------------------------------------------------------------

Table::Table(shared_ptr<Relation> rel)
: rel(std::move(rel)) {
    if (!this->rel)
        throw invalid_argument("Table: relation must not be null");
}

shared_ptr<Relation> Table::getRelation() { return rel; }

void Table::addRecord(string data) {
    // Construct a Record (validates size and domain) then dispatch to the
    // concrete addRecord(Record), which enforces the key-uniqueness constraint.
    addRecord(Record(rel, std::move(data)));
}

// -----------------------------------------------------------------------
// VirtualTable
// -----------------------------------------------------------------------

VirtualTable::VirtualTable(shared_ptr<Relation> rel)
: Table(std::move(rel)) {}

void VirtualTable::addRecord(Record record) {
    // Normalize: validate the incoming bytes against this table's own relation.
    // This rejects records constructed with a different schema and ensures every
    // stored Record shares the table's Relation pointer.
    Record candidate(rel, record.getData());

    // Enforce key uniqueness before inserting.
    if (getRecord(candidate.getKeyData()).has_value())
        throw invalid_argument("VirtualTable: duplicate key violates primary key constraint");
    records.push_back(std::move(candidate));
}

optional<Record> VirtualTable::getRecord(string_view key) {
    if (key.size() != rel->getKeySize())
        throw invalid_argument("VirtualTable::getRecord: key size mismatch");

    for (const Record& r : records) {
        if (r.getKeyData() == key)
            return r;                          // copy by value
    }
    return {};
}

optional<Record> VirtualTable::deleteRecord(string_view key) {
    if (key.size() != rel->getKeySize())
        throw invalid_argument("VirtualTable::deleteRecord: key size mismatch");

    for (auto it = records.begin(); it != records.end(); ++it) {
        if (it->getKeyData() == key) {
            Record result = std::move(*it);
            records.erase(it);
            return result;
        }
    }
    return {};
}

bool VirtualTable::updateRecordByKey(string_view key, const vector<Value>& newValues) {
    if (key.size() != rel->getKeySize())
        throw invalid_argument("VirtualTable::updateRecordByKey: key size mismatch");

    for (Record& record : records) {
        if (record.getKeyData() == key) {
            // Step 1 – build a validated candidate from a copy.
            Record candidate = record;
            for (const Value& val : newValues) {
                candidate.setValue(val);       // validates field, size, domain; throws on failure
            }

            // Step 2 – if the key changed, reject a duplicate new key.
            string_view candidateKey = candidate.getKeyData();
            if (candidateKey != key) {
                for (const Record& r : records) {
                    if (&r != &record && r.getKeyData() == candidateKey) {
                        return false;          // duplicate key; original record unchanged
                    }
                }
            }

            // Step 3 – commit.
            record = std::move(candidate);
            return true;
        }
    }
    return false;                              // key not found
}

vector<Record> VirtualTable::scan() {
    return records;                            // copy all records by value
}

// -----------------------------------------------------------------------
// PhysicalTable
// -----------------------------------------------------------------------

PhysicalTable::PhysicalTable(shared_ptr<Relation> rel, string name, FilePtr file)
: Table(std::move(rel)), name(std::move(name)), file(std::move(file)) {
    if (!this->file)
        throw invalid_argument("PhysicalTable: file must not be null");
}

void PhysicalTable::addRecord(Record record) {
    // Normalize: validate incoming bytes against this table's own relation.
    Record candidate(rel, record.getData());

    // Enforce key uniqueness.
    if (file->getData(candidate.getKeyData()).has_value())
        throw invalid_argument("PhysicalTable: duplicate key violates primary key constraint");

    file->insert(candidate.getData());
}

optional<Record> PhysicalTable::getRecord(string_view key) {
    if (key.size() != rel->getKeySize())
        throw invalid_argument("PhysicalTable::getRecord: key size mismatch");

    auto raw = file->getData(key);
    if (raw.has_value())
        return Record(rel, std::move(raw.value()));
    return {};
}

optional<Record> PhysicalTable::deleteRecord(string_view key) {
    if (key.size() != rel->getKeySize())
        throw invalid_argument("PhysicalTable::deleteRecord: key size mismatch");

    auto raw = file->deleteData(key);
    if (raw.has_value())
        return Record(rel, std::move(raw.value()));
    return {};
}

bool PhysicalTable::updateRecordByKey(string_view key, const vector<Value>& newValues) {
    if (key.size() != rel->getKeySize())
        throw invalid_argument("PhysicalTable::updateRecordByKey: key size mismatch");

    // Step 1 – read the old record; return false if not found.
    auto raw = file->getData(key);
    if (!raw.has_value())
        return false;

    // Step 2 – build a validated candidate.
    Record candidate(rel, std::move(raw.value()));
    for (const Value& val : newValues) {
        candidate.setValue(val);               // validates field, size, domain; throws on failure
    }

    // Step 3 – if the key changed, reject a duplicate new key.
    string_view newKey = candidate.getKeyData();
    if (newKey != key && file->getData(newKey).has_value()) {
        return false;                          // duplicate key; file unchanged
    }

    // Step 4 – commit via in-place overwrite.
    file->updateData(key, candidate.getData());
    return true;
}

vector<Record> PhysicalTable::scan() {
    vector<Record> result;
    for (const string& raw : file->scan())
        result.emplace_back(rel, raw);
    return result;
}

const string& PhysicalTable::getName() const { return name; }
