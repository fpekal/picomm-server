#include <map>
#include <vector>

#include "json.hpp"

class user {
public:

};

class message {
public:
	long long time;
	std::string content;
};

class channel {
public:
	std::vector<message> messages;
};

class chat {
public:
	std::mutex mutex;
	std::map<std::string, user> users;
	std::map<std::string, channel> channels;

	void send_channel(const std::string& user_name, const std::string& channel_name, const std::string& message) {
		std::lock_guard<std::mutex> guard(mutex);
		channel& c = get_channel(channel_name);

		std::string content = std::string{"<"} + user_name + std::string{"> "} + message;
		c.messages.push_back({std::chrono::steady_clock::now().time_since_epoch().count(), content});

		if (c.messages.size() <= 10) return;

		for (int i = 0; i < c.messages.size() - 10; i++) {
			c.messages.erase(c.messages.begin());
		}
	}

	std::vector<message> get_messages_channel(const std::string& channel_name) {
		std::lock_guard<std::mutex> guard(mutex);
		channel& c = get_channel(channel_name);

		auto begin = c.messages.begin();
		auto end = c.messages.end();
		
		if (c.messages.size() > 10) {
			begin = c.messages.end() - 10;
		}

		std::vector<message> result(begin, end);

		return result;
	}

private:
	channel& get_channel(const std::string& channel_name) {
		auto iter = channels.find(channel_name);
		if (iter != channels.end()) return iter->second;

		channels[channel_name] = channel{};

		return channels[channel_name];
	}
} ch;

std::string chat_controller(nlohmann::json j) {
	nlohmann::json ret;

	std::string action = j["action"].get<std::string>();
	
	if (action == "send_channel") {
		try {
			std::string message = j["message"].get<std::string>();
			std::string user = j["user"].get<std::string>();
			std::string channel = j["channel"].get<std::string>();

			ch.send_channel(user, channel, message);
			
			ret = nlohmann::json({{"status", 0}});
		}
		catch( ... ) {
			ret = nlohmann::json({{"status", 1}, {"error", "Bad request"}});
		}
	}
	else if (action == "get_messages_channel") {
		try {
			std::string channel = j["channel"].get<std::string>();

			auto messages = ch.get_messages_channel(channel);
			std::vector<std::string> messages_str;

			for (auto& m : messages) {
				messages_str.push_back(m.content);
			}

			ret = nlohmann::json({{"status", 0}, {"response", {{"messages", messages_str}}}});
		}
		catch( ... ) {
			ret = nlohmann::json({{"status", 1}, {"error", "Bad request"}});
		}
	}

	return ret.dump();
}
