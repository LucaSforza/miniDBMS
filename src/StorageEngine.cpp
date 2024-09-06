#include "StorageEngine.hpp"
#include "Tables.hpp"
#include "File.hpp"

Field::Field(const string& name, shared_ptr<Domain> domain,bool isKeyField)
: name(name), domain(domain), isKeyField(isKeyField) {}

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
    return *domain.get() == *other.domain.get() && name == name;
}

Relation::Relation(vector<Field> fields) {
    this->fields    = vector<Field>();
    this->keyFields = vector<Field>();

    auto names = unordered_set<string_view>();

    recordTotalSize = 0;
    keySize = 0;
    
    for(Field f: fields) {
        if(f.isKey()) {
            keyFields.push_back(f);
            keySize += f.size();
        } else fields.push_back(f);

        if(names.find(f.getName()) == names.end()) {
            names.insert(f.getName());
        } else {
            throw invalid_argument("Non possono esistere due campi con lo stesso nome");
        }

        recordTotalSize += f.size();

    }

    if(this->keyFields.empty())
        throw invalid_argument("Ci deve essere almeno una chiave");
}


size_t Relation::startPointOf(const Field& field) const {
    size_t result = 0;

    for(auto f : keyFields) {
        if(f.getName() == field.getName()) {
            return result;
        }
        result += f.size();
    }

    for(auto f : fields) {
        if(f.getName() == field.getName()) {
            return result;
        }
        result += f.size();
    }

    return result;
}

bool Relation::isValid(const string& data) const {

    size_t i = 0;
    bool result = true;

    for(auto f : keyFields) {
        if(!f.isValid(string_view(data.c_str() + i,f.size()))) {
            return false;
        }
        i += f.size();
    }

    for(auto f : fields) {
        if(!f.isValid(string_view(data.c_str() + i,f.size()))) {
            return false;
        }
        i += f.size();
    }

    return true;
}

const vector<Field>& Relation::getKey() const {
    return keyFields;
}

bool Relation::operator==(const Relation& other) const {
    return fields == other.fields && keyFields == other.keyFields;
}

size_t Relation::getRecordSize() const { return recordTotalSize; }

size_t Relation::getKeySize() const { return keySize; }


Record::Record(shared_ptr<Relation> rel, string data): rel(rel), data(data) {
    if(!rel.get()->isValid(data)) {
        throw invalid_argument("I dati non sono validi");
    }
}

const string& Record::getData() { return data; }

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
    return rel.get()->isValid(data);
}

vector<Value> Record::getKey() const {

    auto result = vector<Value>();

    for(auto key : rel.get()->getKey()) {
        result.push_back(tuple(key,valueAt(key))); //TODO: fare in modo che l'interno del for sia O(1)
    }

    return result;
}

string_view Record::getKeyData() const {
    return string_view(data.c_str(), rel.get()->getKeySize());
}

void Record::setValue(const Value& val) {
    auto [field,newData] = val;
    memcpy(data.data() + rel.get()->startPointOf(field),newData.data(), field.size());
}

Database::Database(string name,string dirPath): name(name), dirPath(dirPath) {
    domains.push_back(make_shared<IntegerDomain>());
    domains.push_back(make_shared<StringDomain>(25));

    if (!fs::exists(dirPath)) {
        fs::create_directory(dirPath);
    }
}

