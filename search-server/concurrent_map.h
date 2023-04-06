#include <map>
#include <string>
#include <mutex>

template<typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value &ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : bucket_count_(bucket_count), concurrent_maps_(bucket_count) {}

    Access operator[](const Key &key) {
        auto &selected_map = concurrent_maps_.at(static_cast<uint64_t>(key) % bucket_count_);
        return {std::lock_guard<std::mutex>(selected_map.mutex), selected_map.map[key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> ordinary_map;
        for (auto& [map, mutex] : concurrent_maps_) {
            std::lock_guard<std::mutex> guard(mutex);
            ordinary_map.insert(map.begin(), map.end());
        }
        return ordinary_map;
    }

private:

    struct MapAndLock {
        std::map<Key, Value> map;
        std::mutex mutex;
    };

    size_t bucket_count_;
    std::vector<MapAndLock> concurrent_maps_;
};
