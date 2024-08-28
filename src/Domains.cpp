#include "Domains.hpp"

EnumDomain::EnumDomain(const std::vector<std::string>& validValues) : validValues(validValues) {
    max_len = 0;
    for(std::string val: validValues) {
        if(val.size() > max_len)
            max_len = val.size();
    }
}

bool EnumDomain::isValid(const std::string_view value) const {
    return std::find(validValues.begin(), validValues.end(), value) != validValues.end();
}

size_t EnumDomain::size() const {
    return max_len;
}

bool EnumDomain::operator==(const Domain& other) const {
    if (typeid(*this) != typeid(other)) {
        return false;
    }
        
    const auto& derived = static_cast<const EnumDomain&>(other);
        
    return max_len == derived.max_len && validValues == derived.validValues;
}

bool IntegerDomain::isValid(const std::string_view value) const {
    return value.length() == sizeof(int); // deve essere lungo 4 byte per essere un numero valido
}

size_t IntegerDomain::size() const {
    return sizeof(int);
}

bool IntegerDomain::operator==(const Domain& other) const {
    return typeid(*this) == typeid(other);
}

StringDomain::StringDomain(size_t max_len) : max_len(max_len) {}

bool StringDomain::isValid(const std::string_view value) const {
    return value.length() <= max_len;
};

size_t StringDomain::size() const {
    return max_len;
}

bool StringDomain::operator==(const Domain& other) const {
    if (typeid(*this) != typeid(other))
        return false;
        
    const auto& derived = static_cast<const StringDomain&>(other);

    return max_len == derived.max_len;
}