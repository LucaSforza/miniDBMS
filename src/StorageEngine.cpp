#include <cstring>

#include "StorageEngine.hpp"
#include "Tables.hpp"
#include "File.hpp"

Field::Field(const string& name, shared_ptr<Domain> domain,bool isKeyField)
: name(name), domain(domain), isKeyField(isKeyField) {
    if(!domain) {
        throw invalid_argument("Il dominio di un campo non può essere nullo");
    }
}

Field::Field(const string& name, shared_ptr<Domain> domain)
: Field(name,domain,false) {}

const string& Field::getName() const { return name; }

SharedDomain Field::getDomain() const { return domain; }

bool Field::isKey() const { return isKeyField; }

bool Field::isValid(const string_view value) const {
    return domain->isValid(value);
}

size_t Field::size() const {
    return domain.get()->size();
}

bool Field::operator==(const Field& other) const {
    // Compare domain identity, field name, and key-vs-non-key role.
    // Two fields with the same name and domain but different key status
    // belong to different schemas.
    return *domain.get() == *other.domain.get()
        && name == other.name
        && isKeyField == other.isKeyField;
}

Relation::Relation(vector<Field> fieldList) {
    // First pass: validate all field names are unique
    unordered_set<string_view> names;
    for(const auto& f : fieldList) {
        if(!names.insert(f.getName()).second) {
            throw invalid_argument("Non possono esistere due campi con lo stesso nome");
        }
    }

    // Separate key and non-key fields without mutating the input
    vector<Field> keys, nonKeys;
    for(auto& f : fieldList) {
        if(f.isKey()) {
            keys.push_back(std::move(f));
        } else {
            nonKeys.push_back(std::move(f));
        }
    }

    // Require at least one key
    if(keys.empty()) {
        throw invalid_argument("Ci deve essere almeno una chiave");
    }

    // Build the ordered field list: all keys first, then non-keys.
    // This determines the fixed-width record layout.
    this->fields = std::move(keys);
    this->keyFields = this->fields;  // copy the key prefix for getKey()
    this->fields.insert(this->fields.end(),
                        std::make_move_iterator(nonKeys.begin()),
                        std::make_move_iterator(nonKeys.end()));

    // Compute sizes by iterating the ordered all-fields list
    recordTotalSize = 0;
    keySize = 0;
    for(const auto& f : this->fields) {
        recordTotalSize += f.size();
        if(f.isKey()) {
            keySize += f.size();
        }
    }
}


size_t Relation::startPointOf(const Field& field) const {
    size_t offset = 0;

    // fields contains all fields in key-first order, matching record layout.
    // Membership requires full Field equality (name, domain, key status) so
    // an external field with the same name but different domain/size/role is
    // rejected rather than silently using the relation's offset.
    for(const auto& f : fields) {
        if(f == field) {
            return offset;
        }
        offset += f.size();
    }

    throw invalid_argument("Il campo non appartiene alla relazione");
}

bool Relation::isValid(const string& data) const {
    // Reject data whose byte length does not match the expected record size
    // before constructing any field string_views.  This prevents out-of-bounds
    // pointer arithmetic when a caller passes a short or long buffer.
    if(data.size() != recordTotalSize) {
        return false;
    }

    size_t offset = 0;

    // fields contains all fields (keys first, then non-keys), matching the
    // fixed-width record layout.
    for(const auto& f : fields) {
        if(!f.isValid(string_view(data.c_str() + offset, f.size()))) {
            return false;
        }
        offset += f.size();
    }

    return true;
}

const vector<Field>& Relation::getKey() const {
    return keyFields;
}

bool Relation::operator==(const Relation& other) const {
    // fields contains all fields (keys first, then non-keys),
    // so comparing fields alone checks the entire schema.
    return fields == other.fields;
}

size_t Relation::getRecordSize() const { return recordTotalSize; }

size_t Relation::getKeySize() const { return keySize; }


Record::Record(shared_ptr<Relation> rel, string data): rel(rel), data(data) {
    if(!rel) {
        throw invalid_argument("Il record richiede una relazione non nulla");
    }
    if(!isValid()) {
        throw invalid_argument("I dati non sono validi");
    }
}

const string& Record::getData() const { return data; }

    // Ritorna una vista sulla parte di record di cui fa parte il campo
const string_view Record::valueAt(const Field& field) const {
    return string_view(data.c_str() + rel.get()->startPointOf(field), field.size());
}

bool Record::valuesInside(const vector<Value>& values) const {
    for(auto [field,data] : values) {
        if(valueAt(field) != data)
            return false;
    }

    return true;
}

bool Record::isValid() const {
    // Require exact byte length before examining individual fields.
    // This protects against slicing records that are too short or too long.
    return data.size() == rel->getRecordSize() && rel->isValid(data);
}

vector<Value> Record::getKey() const {

    auto result = vector<Value>();

    // Iterate by const reference to the Relation-owned key vector so that
    // each Value tuple stores a reference to a stable Field, not a loop-local copy.
    for(const auto& key : rel->getKey()) {
        result.push_back(Value(key, valueAt(key))); //TODO: fare in modo che l'interno del for sia O(1)
    }

    return result;
}

string_view Record::getKeyData() const {
    return string_view(data.c_str(), rel.get()->getKeySize());
}

void Record::setValue(const Value& val) {
    auto [field, newData] = val;

    // Step 1: verify the field belongs to this relation.
    // startPointOf throws invalid_argument if the field is not found.
    size_t offset = rel->startPointOf(field);

    // Step 2: reject values whose byte size does not match the field slot.
    if(newData.size() != field.size()) {
        throw invalid_argument("La dimensione del valore non corrisponde al campo");
    }

    // Step 3: reject values that violate domain constraints.
    if(!field.isValid(newData)) {
        throw invalid_argument("Il valore non soddisfa i vincoli del dominio");
    }

    // Step 4: only now mutate the record.
    memcpy(data.data() + offset, newData.data(), field.size());
}

Database::Database(string name,string dirPath): name(name), dirPath(dirPath) {
    domains.push_back(make_shared<IntegerDomain>());
    domains.push_back(make_shared<StringDomain>(25));

    if (!fs::exists(dirPath)) {
        fs::create_directory(dirPath);
    }
}
