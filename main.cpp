#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <map>
#include <chrono>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "json.hpp"
#include "base64.h"

#include "chat_controller.h"

struct command_merge_entry {
	int client = 0;
	long long time = 0;
	int id = 0;
	int size = 0;
	std::map<int, std::string> order;
};

struct command_merge_entry_part {
	int client = 0;
	long long time = 0;
	int id = 0;
	int size = 0;
	int order = 0;
	std::string payload;
};

std::string un_escape(const std::string& str) {
	std::string result;

	for (auto iter = str.begin(); iter != str.end(); ++iter) {
		if (*iter != '\\') result += *iter;
		else {
			++iter;
			result += *iter;
		}
	}

	return result;
}

void add_command(std::map<int, command_merge_entry>& commands_parts, command_merge_entry_part& cm) {
	auto iter = commands_parts.find(cm.id);
	if (iter == commands_parts.end()) {
		command_merge_entry entry;
		entry.id = cm.id;
		entry.time = cm.time;
		entry.size = cm.size;
		entry.order[cm.order] = cm.payload;
		if (cm.client != 0) {
			entry.client = cm.client;
		}
		commands_parts[cm.id] = entry;
	} else {
		iter->second.order[cm.order] = cm.payload;
		if (cm.client != 0) {
			iter->second.client = cm.client;
		}
	}

	for (auto& c : commands_parts) {
		if (c.second.order.size() == c.second.size) {
			std::cout << "Command " << c.second.id << " completed" << std::endl;

			std::stringstream ss;

			for (auto& o : c.second.order) {
				ss << o.second;
			}
			std::string result = ss.str();
			for (int i = 0; i < 2; i++) {
				auto pos = result.find("%5e");
				if (pos == -1) break;

				result.replace(pos, 3, "=");
			}

			result = base64_decode(result);
			result.erase(std::find_if(result.rbegin(), result.rend(), std::bind_front(std::not_equal_to<char>(), '\0')).base(), result.end());
			result = result.substr(1, result.length() - 2);
			result = un_escape(result);
			std::cout << result << '\n';
			nlohmann::json j = nlohmann::json::parse(result);

			const char* header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
			write(c.second.client, header, strlen(header));

			std::string reply = chat_controller(j);
			write(c.second.client, reply.c_str(), reply.length());
			header = "\r\n\r\n";
			write(c.second.client, header, strlen(header));
			close(c.second.client);



			commands_parts.erase(c.first);
			break;
		}
	}
}

command_merge_entry_part parse_request(const std::string& req) {
	size_t pos = req.find("?id=");
	size_t pos2 = req.find("&", pos);
	std::string id = req.substr(pos + 4, pos2 - pos - 4);

	pos = req.find("&s=");
	pos2 = req.find("&", pos+1);
	std::string size = req.substr(pos + 3, pos2 - pos - 3);

	pos = req.find("&o=");
	pos2 = req.find("&", pos+1);
	std::string order = req.substr(pos + 3, pos2 - pos - 3);

	pos = req.find("&p=");
	pos2 = req.find(" ", pos+1);
	std::string payload = req.substr(pos + 3, pos2 - pos - 3);

	try{
		command_merge_entry_part cm;
		cm.id = std::stoi(id);
		cm.size = std::stoi(size);
		cm.order = std::stoi(order);
		cm.payload = payload;
		cm.time = std::chrono::steady_clock::now().time_since_epoch().count();

		std::cout << "id=" << cm.id << " size=" << cm.size << " order=" << cm.order << std::endl;

		return cm;
	}
	catch(...) {
		command_merge_entry_part cm;
		cm.id = -1;

		return cm;
	}
}

void connection_handler(std::vector<int>& connections_queue, std::mutex& connections_queue_mutex, std::map<int, command_merge_entry>& commands_parts, std::mutex& commands_parts_mutex) {
	char buf[2048];

	while(true) {
		connections_queue_mutex.lock();

		if (connections_queue.size() == 0) {
			connections_queue_mutex.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		int client = connections_queue.back();
		connections_queue.pop_back();
		connections_queue_mutex.unlock();
		std::cout << "Client handling\n";

		int bytes_read = read(client, buf, sizeof(buf));
		if (strncmp(buf, "GET", 3) != 0) {
			std::cerr << "Wrong request" << std::endl;
			continue;
		}

		auto parse_result = parse_request(std::string(buf, bytes_read));

		if (parse_result.id == -1) {
			std::cerr << "Wrong request" << std::endl;
			continue;
		}

		if (parse_result.order == parse_result.size) {
			if (parse_result.client != 0) {
				std::cerr << "Wrong request" << std::endl;
				close(client);
				close(parse_result.client);
				continue;
			}
			parse_result.client = client;
		}
		else {
			const char* header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
			write(client, header, strlen(header));
			close(client);
			std::cout << "Client closed\n";
		}

		commands_parts_mutex.lock();
		add_command(commands_parts, parse_result);
		commands_parts_mutex.unlock();
	}
}

int main() {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	assert(sock != -1);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(5002);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	bind(sock, (sockaddr*)&addr, sizeof(addr));

	listen(sock, 64);

	std::vector<int> connections_queue;
	std::mutex connections_queue_mutex;

	std::map<int, command_merge_entry> commands_parts;
	std::mutex commands_parts_mutex;

	std::array<std::jthread, 64> threads;
	for (auto& thread : threads) {
		thread = std::jthread(connection_handler, std::ref(connections_queue), std::ref(connections_queue_mutex), std::ref(commands_parts), std::ref(commands_parts_mutex));
	}

	while (true) {
		sockaddr_in client_addr;
		socklen_t client_addr_size = sizeof(client_addr);
		int client = accept(sock, (sockaddr*)&client_addr, &client_addr_size);
		std::cout << "Client accepted\n";

		connections_queue_mutex.lock();
		connections_queue.push_back(client);
		connections_queue_mutex.unlock();
	}

	return 0;
}
