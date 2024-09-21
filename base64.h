#include <string>

int char_to_int(char c) {
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	return 0;
}

char int_to_char(int i) {
	if (i >= 0 && i <= 25) return i + 'A';
	if (i >= 26 && i <= 51) return i - 26 + 'a';
	if (i >= 52 && i <= 61) return i - 52 + '0';
	return 0;
}

std::string base64_decode(const std::string& data) {
	std::string ret;

	for (int i = 0; i < data.size(); i += 4) {
		int a = char_to_int(data[i]);
		int b = char_to_int(data[i + 1]);
		int c = char_to_int(data[i + 2]);
		int d = char_to_int(data[i + 3]);

		ret += char(a << 2 | b >> 4);
		ret += char((b & 0xf) << 4 | c >> 2);
		ret += char((c & 0x3) << 6 | d);
	}

	return ret;
}

std::string base64_encode(const std::string& data) {
	std::string ret;
	int a, b, c;
	for (int i = 0; i < data.size(); i += 3) {
		a = data[i];
		b = i + 1 < data.size() ? data[i + 1] : 0;
		c = i + 2 < data.size() ? data[i + 2] : 0;
		ret += int_to_char(a >> 2);
		ret += int_to_char((a & 0x3) << 4 | b >> 4);
		ret += int_to_char((b & 0xf) << 2 | c >> 6);
		ret += int_to_char(c & 0x3f);
	}
	if (data.size() % 3 == 2) {
		ret += int_to_char(data[data.size() - 2] >> 2);
		ret += int_to_char((data[data.size() - 2] & 0x3) << 4 | data[data.size() - 1] >> 4);
		ret += int_to_char((data[data.size() - 1] & 0xf) << 2);
		ret += '=';
	}
	if (data.size() % 3 == 1) {
		ret += int_to_char(data[data.size() - 1] >> 2);
		ret += int_to_char((data[data.size() - 1] & 0x3) << 4);
		ret += "==";
	}
	return ret;
}
