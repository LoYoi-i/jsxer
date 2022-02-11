#include "reader.h"
#include "util.h"

using namespace jsxbin;

Reader::Reader(const string& jsxbin) {
    size_t input_len = jsxbin.length();

    _data.resize(input_len);
    memcpy(_data.data(), jsxbin.c_str(), input_len);

    _start = _cursor = 0;
    _end = input_len - 1;
    _depth = 0;

    _error = ParseError::None;
    _version = JsxbinVersion::Invalid;
}

JsxbinVersion Reader::version() const {
    return _version;
}

ParseError Reader::error() const {
    return _error;
}

size_t Reader::depth() const {
    return _depth;
}

void Reader::step(int offset) {
    _cursor += offset;
}

Token Reader::peek(int offset) {
    return _data[_cursor + offset];
}

size_t Reader::get_node_depth() {
    if (_depth == 0){
        update_node_depth();
    }

    return _depth;
}

int Reader::parse_node_depth() {
    Token current = peek();

    if (current == 'A') {
        step();
        return 1;

    } else if (current == '0') {
        step();
        int levels = get() - 0x3f;

        if (levels > 0x1b) {
            return levels + parse_node_depth();
        }
        return levels;

    }

    return 0;
}

void Reader::update_node_depth() {
    _depth = parse_node_depth();
}

bool Reader::decrement_node_depth() {
    if (get_node_depth() == 0)
        return false;

    _depth--;
    return true;
}

bool Reader::verifySignature() {
    if ( utils::bytes_eq((uint8_t*) _data.data(), (uint8_t*) JSXBIN_SIGNATURE_V10, JSXBIN_SIGNATURE_LEN) ) {
        _version = JsxbinVersion::v10;
    } else if ( utils::bytes_eq((uint8_t*) _data.data(), (uint8_t*) JSXBIN_SIGNATURE_V20, JSXBIN_SIGNATURE_LEN) ) {
        _version = JsxbinVersion::v20;
    } else if ( utils::bytes_eq((uint8_t*) _data.data(), (uint8_t*) JSXBIN_SIGNATURE_V21, JSXBIN_SIGNATURE_LEN) ) {
        _version = JsxbinVersion::v21;
    } else {
        _error = ParseError::InvalidVersion;
        printf("[!]: %s\n", "Parse Error at verifySignature()");

        return false;
    }

    _start = _cursor += JSXBIN_SIGNATURE_LEN;

    return true;
}

Token Reader::get() {
    Token token = _next();

    while (_ignorable(token)) {
        token = _next();
    }

    return token;
}

Byte Reader::getByte() {
    if (_depth > 0) {
        --_depth;
        return 0;
    }

    Token m = get();

    if (m == '0') {
        Token n = get();

        if ((n - 0x41) > 0x19) {
            goto error8;
        } else {
            _depth = n - 0x40;
            return 0;
        }
    } else if ((m - 0x41) > 0x19) {
        if ((m - 0x67) > 7) {
            goto error8;
        } else {
            Token z = get();
            uint8_t l, r = 32 * (m + 1);

            if ((z - 0x41) > 0x19) {
                if ((z - 0x61) > 5) {
                    goto error8;
                } else {
                    l = z - 0x47;
                }
            } else {
                l = z - 0x41;
            }

            return (l | r);
        }
    }

    return m - 0x41;

error8:
    _error = ParseError::Error8;
    printf("[!]: %s\n", "Parse Error at getByte()");
    return 0;
}

Number Reader::getNumber() {
    if (_depth > 0) {
        --_depth;
        return 0.0;
    }

    Token t = get();
    Number res = 0, sign = (t != 'y') ? 1.0 : (t = get(), -1.0);

    switch (t) {
        case '2':
        case '4':
        case '8': {
            for (int i = 0; i < t - 48; ++i) {
                ((Byte*) &res)[i] = getByte();
            }
            break;
        }
        default: {
            step(-1);
            res = getByte();
            break;
        }
    }

    return sign * res;
}

ByteString Reader::getString() {
    ByteString result;

    int length = (int) getNumber();

    for (int i = 0; i < length; ++i) {
        // Each char is a unicode (utf-16) codepoint.
        result.push_back((uint16_t) getNumber());
    }

    return result;
}

bool Reader::getBoolean() {
    Token t = get();

    if (t == 't') {
        return true;
    } else if (t == 'f') {
        return false;
    } else {
        _error = ParseError::Error8;
        printf("[!]: %s\n", "Parse Error at getBoolean()");
    }

    return false;
}

ByteString Reader::readSID() {
    Token t = get();

    ByteString symbol;
    Number id;

    if (t == 'z') {
        symbol = getString();
        id = getNumber();
        addSymbol((int) id, symbol);
    } else {
        id = getNumber();
        symbol = getSymbol((int) id);
    }

    return symbol;
}

Variant* Reader::getVariant() {
    if (peek() == 'n') {
        return nullptr;
    }

    uint8_t type = get() - 'a';

    auto* result = new Variant();
    switch (type) {
        case 0: // 'a' - also recognized as a null at runtime.
            // looks like it meant for undefined
            // but not utilized.
            result->doErase();

            // TODO: find a better way for this
            result->setNull();
            break;
        case 1: // 'b' - null always encoded to 'b'
            // null type
            result->setNull();
            break;
        case 2: // 'c'
            // Boolean type
            result->setBool(getBoolean());
            break;
        case 3: // 'd'
            // Number type
            result->setDouble(getNumber());
            break;
        case 4: // 'e'
            // String type
            result->setString(getString());
            break;

        default:
            _error = ParseError::Error8;
            printf("[!]: %s\n", "Parse Error at getVariant()");
            break;
    }

    return result;
}

string DataPool::get(const string &key) {
    return _pool.at(key);
}

void DataPool::add(const string &key, string value) {
    _pool[key] = std::move(value);
}

void DataPool::clear() {
    _pool.clear();
}

Token Reader::_next() {
    if (_cursor < _end) {
        return _data[_cursor++];
    }

    _error = ParseError::ReachedEnd;

    return _data[_end];
}

bool Reader::_ignorable(Token value) {
    switch (static_cast<char>(value)) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return true;
        default:
            return false;
    }
}

void Reader::addSymbol(int id, const ByteString& symbol) {
    _symbols[id] = symbol;
}

ByteString Reader::getSymbol(int id) {
    return _symbols[id];
}

Variant::Variant() {
    _type = VariantType::None;
    doErase();
}

void Variant::setNull() {
    _type = VariantType::Null;
    doErase();
}

void Variant::setBool(bool value) {
    _type = VariantType::Boolean;
    doErase();
    _value._bool = value;
}

void Variant::setDouble(double value) {
    _type = VariantType::Number;
    doErase();
    _value._double = value;
}

void Variant::setString(const ByteString& value) {
    _type = VariantType::String;
    doErase();
    _value._string = value;
}

void Variant::doErase() {
    if (_type == VariantType::String) {
        _value._string.clear();
    }

    utils::zero_mem(&_value, sizeof(ValueType));
}

String Variant::toString() {
    switch (_type) {
        case VariantType::Undefined: return "undefined";
        case VariantType::Null: return "null";
        case VariantType::Boolean:
            return _value._bool ? "true" : "false";
        case VariantType::Number:
            return utils::number_to_string(_value._double);
        case VariantType::String:
            return utils::to_string_literal(_value._string);
        default:
            return "";
    }
}
