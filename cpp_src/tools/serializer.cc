#include "tools/serializer.h"
#include <stdarg.h>
#include <stdio.h>
#include <cstring>
#include "itoa/itoa.h"

#include "tools/varint.h"
namespace reindexer {

Serializer::Serializer(const void *_buf, int _len) : buf(static_cast<const uint8_t *>(_buf)), len(_len), pos(0) {}

bool Serializer::Eof() { return pos >= len; }

KeyValue Serializer::GetValue() {
	switch (GetInt()) {
		case KeyValueInt:
			return KeyValue(GetInt());
		case KeyValueInt64:
			return KeyValue(GetInt64());
		case KeyValueDouble:
			return KeyValue(GetDouble());
		case KeyValueString:
			return KeyValue(GetString());
		default:
			abort();
	}
}

KeyRef Serializer::GetRef() {
	switch (GetInt()) {
		case KeyValueInt:
			return KeyRef(GetInt());
		case KeyValueInt64:
			return KeyRef(GetInt64());
		case KeyValueDouble:
			return KeyRef(GetDouble());
		case KeyValueString:
			return KeyRef(GetPString());
		default:
			abort();
	}
}

string Serializer::GetString() {
	int l = GetInt();
	const char *ret = reinterpret_cast<const char *>(buf + pos);
	assert(pos + l <= len);
	pos += l;
	return string(ret, l);
}

p_string Serializer::GetPString() {
	auto ret = reinterpret_cast<const l_string_hdr *>(buf + pos);
	int l = GetInt();
	assert(pos + l <= len);
	pos += l;
	return p_string(ret);
}

Slice Serializer::GetSlice() {
	int l = GetInt();
	Slice b(reinterpret_cast<const char *>(buf + pos), l);
	assert(pos + b.size() <= len);
	pos += b.size();
	return b;
}

int Serializer::GetInt() {
	int ret;
	assert(pos + sizeof(ret) <= len);
	memcpy(&ret, buf + pos, sizeof(ret));
	pos += sizeof(ret);
	return ret;
}

int64_t Serializer::GetInt64() {
	int64_t ret;
	assert(pos + sizeof(ret) <= len);
	memcpy(&ret, buf + pos, sizeof(ret));
	pos += sizeof(ret);
	return ret;
}

double Serializer::GetDouble() {
	double ret;
	assert(pos + sizeof(ret) <= len);
	memcpy(&ret, buf + pos, sizeof(ret));
	pos += sizeof(ret);
	return ret;
}

int64_t Serializer::GetVarint() {
	int l = scan_varint(len - pos, buf + pos);
	assert(pos + l <= len);
	pos += l;
	return unzigzag64(parse_uint64(l, buf + pos - l));
}
uint64_t Serializer::GetVarUint() {
	int l = scan_varint(len - pos, buf + pos);
	assert(pos + l <= len);
	pos += l;
	return parse_uint64(l, buf + pos - l);
}

Slice Serializer::GetVString() {
	int l = GetVarUint();
	assert(pos + l <= len);
	pos += l;
	return Slice(reinterpret_cast<const char *>(buf + pos - l), l);
}

p_string Serializer::GetPVString() {
	auto ret = reinterpret_cast<const v_string_hdr *>(buf + pos);
	int l = GetVarUint();
	assert(pos + l <= len);
	pos += l;
	return p_string(ret);
}

bool Serializer::GetBool() { return bool(GetVarUint()); }

WrSerializer::WrSerializer(bool allowInBuf)
	: buf_(allowInBuf ? this->inBuf_ : nullptr), len_(0), cap_(allowInBuf ? sizeof(this->inBuf_) : 0) {}

WrSerializer::~WrSerializer() {
	if (buf_ != inBuf_) free(buf_);
}

void WrSerializer::PutValue(const KeyValue &kv) {
	PutInt(kv.Type());
	switch (kv.Type()) {
		case KeyValueInt:
			PutInt(kv.toInt());
			break;
		case KeyValueInt64:
			PutInt64(kv.toInt64());
			break;
		case KeyValueDouble:
			PutDouble(kv.toDouble());
			break;
		case KeyValueString:
			PutString(kv.toString());
			break;
		default:
			abort();
	}
}

void WrSerializer::PutString(const string &str) { PutSlice(Slice(str)); }

void WrSerializer::PutSlice(const Slice &slice) {
	PutInt(slice.size());
	grow(slice.size());
	memcpy(&buf_[len_], slice.data(), slice.size());
	len_ += slice.size();
}

void WrSerializer::PutInt(int v) {
	grow(sizeof v);
	memcpy(&buf_[len_], &v, sizeof v);
	len_ += sizeof v;
}
void WrSerializer::PutUInt8(uint8_t v) {
	grow(sizeof v);
	buf_[len_++] = v;
}
void WrSerializer::PutInt8(int8_t v) {
	grow(sizeof v);
	buf_[len_++] = v;
}
void WrSerializer::PutInt16(int16_t v) {
	grow(sizeof v);
	memcpy(&buf_[len_], &v, sizeof v);
	len_ += sizeof v;
}

void WrSerializer::PutInt64(int64_t v) {
	grow(sizeof v);
	memcpy(&buf_[len_], &v, sizeof v);
	len_ += sizeof v;
}

void WrSerializer::PutUInt64(uint64_t v) {
	grow(sizeof v);
	memcpy(&buf_[len_], &v, sizeof v);
	len_ += sizeof v;
}

void WrSerializer::PutDouble(double v) {
	grow(sizeof v);
	memcpy(&buf_[len_], &v, sizeof v);
	len_ += sizeof v;
}

void WrSerializer::grow(size_t sz) {
	if (len_ + sz > cap_ || !buf_) {
		Reserve((cap_ * 2) + sz + 0x1000);
	}
}

void WrSerializer::Reserve(size_t cap) {
	if (cap > cap_ || !buf_) {
		cap_ = std::max(cap, cap_);
		uint8_t *b = reinterpret_cast<uint8_t *>(malloc(cap_));
		if (!b) throw std::bad_alloc();
		if (buf_) {
			memcpy(b, buf_, len_);
			if (buf_ != inBuf_) free(buf_);
		}
		buf_ = b;
	}
}

void WrSerializer::Printf(const char *fmt, ...) {
	grow(100);
	va_list args;
	va_start(args, fmt);
	len_ += vsnprintf(reinterpret_cast<char *>(buf_ + len_), len_ - cap_, fmt, args);
	va_end(args);
}

void WrSerializer::PutVarint(int64_t v) {
	grow(10);
	len_ += sint64_pack(v, buf_ + len_);
}

void WrSerializer::PutVarUint(uint64_t v) {
	grow(10);
	len_ += uint64_pack(v, buf_ + len_);
}

void WrSerializer::PutBool(bool v) {
	grow(1);
	len_ += boolean_pack(v, buf_ + len_);
}

void WrSerializer::PutVString(const char *str) {
	grow(strlen(str) + 10);
	len_ += string_pack(str, buf_ + len_);
}

void WrSerializer::PutVString(const Slice &str) {
	grow(str.size() + 10);
	len_ += string_pack(str.data(), str.size(), buf_ + len_);
}

void WrSerializer::PrintJsonString(const Slice &str) {
	const char *s = str.data();
	size_t l = str.size();
	grow(l * 6 + 3);
	char *d = reinterpret_cast<char *>(buf_ + len_);
	*d++ = '"';

	while (l--) {
		int c = *s++;
		switch (c) {
			case '\b':
				*d++ = '\\';
				*d++ = 'b';
				break;
			case '\f':
				*d++ = '\\';
				*d++ = 'f';
				break;
			case '\n':
				*d++ = '\\';
				*d++ = 'n';
				break;
			case '\r':
				*d++ = '\\';
				*d++ = 'r';
				break;
			case '\t':
				*d++ = '\\';
				*d++ = 't';
				break;
			case '\\':
				*d++ = '\\';
				*d++ = '\\';
				break;
			case '"':
				*d++ = '\\';
				*d++ = '"';
				break;
			case '&':
				*d++ = '\\';
				*d++ = 'u';
				*d++ = '0';
				*d++ = '0';
				*d++ = '2';
				*d++ = '6';
				break;
			default:
				*d++ = c;
		}
	}
	*d++ = '"';
	len_ = d - reinterpret_cast<char *>(buf_);
}

void WrSerializer::Print(int k) {
	grow(32);
	char *b = i32toa(k, reinterpret_cast<char *>(buf_ + len_));
	len_ = b - reinterpret_cast<char *>(buf_);
}

void WrSerializer::Print(int64_t k) {
	grow(32);
	char *b = i64toa(k, reinterpret_cast<char *>(buf_ + len_));
	len_ = b - reinterpret_cast<char *>(buf_);
}

uint8_t *WrSerializer::DetachBuffer() {
	uint8_t *b = buf_;
	buf_ = nullptr;
	cap_ = 0;
	return b;
}

uint8_t *WrSerializer::Buf() { return buf_; }

}  // namespace reindexer
