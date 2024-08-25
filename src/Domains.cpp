#include <string>
#include <vector>

// Classe base per un dominio
class Domain {
public:
    virtual ~Domain() = default;
    virtual bool isValid(const std::string_view value) const = 0;
    virtual size_t size() const = 0;
    virtual bool operator==(const Domain& other) const = 0;

};

class EnumDomain : public Domain {
public:
    EnumDomain(const std::vector<std::string>& validValues) : validValues(validValues) {
        max_len = 0;
        for(std::string val: validValues) {
            if(val.size() > max_len)
                max_len = val.size();
        }
    }

    bool isValid(const std::string_view value) const override {
        return std::find(validValues.begin(), validValues.end(), value) != validValues.end();
    }

    size_t size() const override {
        return max_len;
    }

    bool operator==(const Domain& other) const override {
        if (typeid(*this) != typeid(other)) {
            return false;
        }
        
        const auto& derived = static_cast<const EnumDomain&>(other);
        
        return max_len == derived.max_len && validValues == derived.validValues;
    }

private:
    std::vector<std::string> validValues;
    size_t max_len;
};

using SharedDomain = std::shared_ptr<Domain>;

class IntegerDomain : public Domain {
public:

    bool isValid(const std::string_view value) const override {
        return value.length() == sizeof(int); // deve essere lungo 4 byte per essere un numero valido
    }

    size_t size() const override {
        return sizeof(int);
    }

    bool operator==(const Domain& other) const override {
        return typeid(*this) == typeid(other);
    }

};

class StringDomain : public Domain {
public:

    StringDomain(uint max_len) : max_len(max_len) {}

    bool isValid(const std::string_view value) const override {
        return value.length() <= max_len;
    };

    size_t size() const override {
        return max_len;
    }

    bool operator==(const Domain& other) const override {
        if (typeid(*this) != typeid(other))
            return false;
        
        const auto& derived = static_cast<const StringDomain&>(other);

        return max_len == derived.max_len;
    }

private:
    size_t max_len;
};

//TODO: aggiungere domini