
#ifndef _NVDS_CLIENT_H_
#define _NVDS_CLIENT_H_

#include <string>

namespace nvds {

	class Client {
	public:
		Client(const std::string& coord_addr);
		~Client();

		// Get value by the key, return empty string if error occurs.
		// Throw: TransportException
		std::string Get(const std::string& key) {
			return Get(key.c_str(), key.size());
		}
		std::string Get(const char* key, size_t key_len);

		// Insert key/value pair to the cluster, return if operation succeed.
		// Throw: TransportException
		bool Put(const std::string& key, const std::string& val) { return true; }
		bool Put(const char* key, size_t key_len, const char* val, size_t val_len) {
			return true;
		}

		// Add key/value pair to the cluster,
		// return false if there is already the same key;
		// Throw: TransportException
		bool Add(const std::string& key, const std::string& val) {
			return true;
		}
		bool Add(const char* key, size_t key_len, const char* val, size_t val_len) {
			return true;
		}

		// Delete item indexed by the key, return if operation succeed.
		// Throw: TransportException
		bool Del(const std::string& key) {
			return true;
		}
		bool Del(const char* key, size_t key_len) {
			return true;
		}

		// Statistic
		size_t num_send() const { return num_send_; }

	private:
		static const uint32_t kMaxIBQueueDepth = 128;
		static const uint32_t kSendBufSize = 1024 * 2 + 128;
		static const uint32_t kRecvBufSize = 1024 + 128;
		// May throw exception `boost::system::system_error`
	
		size_t num_send_{ 0 };
	};

} // namespace nvds

#endif // _NVDS_CLIENT_H_