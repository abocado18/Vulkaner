#pragma once
#include "dynamic_bitset.h"
#include "thread_pool.h"
#include <cassert>
#include <cinttypes>
#include <functional>
#include <memory>
#include <thread>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cassert>

namespace vecs {
using Entity = uint32_t;
constexpr Entity NO_ENTITY = UINT32_MAX;

class Ecs; // Forward Decl

struct Schedule {

  Schedule() {};
  ~Schedule() = default;

  std::unordered_set<uint32_t> systems;
};

template <typename T> struct Write {
  using type = T;
};

template <typename T> struct Read {
  using type = T;
};

template <typename T> struct unwrap_component {
  using type = T;
};

template <typename T> struct unwrap_component<Read<T>> {
  using type = T;
};

template <typename T> struct unwrap_component<Write<T>> {
  using type = T;
};

template <typename T> using component_t = typename unwrap_component<T>::type;

template <typename T> struct lambda_type {
  using type = typename unwrap_component<T>::type &; // default: writable
};

template <typename T> struct lambda_type<Read<T>> {
  using type = const T &; // read-only
};

template <typename T> struct lambda_type<Write<T>> {
  using type = T &; // writable
};

template <typename T> using lambda_t = typename lambda_type<T>::type;

template <typename T> struct is_read_or_write : std::false_type {};

template <typename T> struct is_read_or_write<Read<T>> : std::true_type {};

template <typename T> struct is_read_or_write<Write<T>> : std::true_type {};

template <typename T> struct is_read : std::false_type {};

template <typename T> struct is_read<Read<T>> : std::true_type {};

template <typename T> struct is_read<Write<T>> : std::false_type {};

template <typename T> struct is_write : std::false_type {};

template <typename T> struct is_write<Write<T>> : std::true_type {};

template <typename T> struct ResMut {
  using type = T;
};

template <typename T> struct Res {
  using type = T;
};

template <typename T> struct unwrapResource {
  using type = T;
};

template <typename T> struct unwrapResource<ResMut<T>> {
  using type = T;
};

template <typename T> struct unwrapResource<Res<T>> {
  using type = T;
};

template <typename T> struct resourceRef {
  using type = typename unwrapResource<T>::type *;
};

template <typename T> struct resourceRef<Res<T>> {
  using type = const T *;
};

template <typename T> struct resourceRef<ResMut<T>> {
  using type = T *;
};

template <typename T> using resource_r = typename resourceRef<T>::type;

template <typename T> struct isMutableResource : std::false_type {};

template <typename T> struct isMutableResource<ResMut<T>> : std::true_type {};

template <typename T> struct isConstResource : std::false_type {};

template <typename T> struct isConstResource<Res<T>> : std::true_type {};

template <typename T> struct isResource : std::false_type {};

template <typename T> struct isResource<Res<T>> : std::true_type {};

template <typename T> struct isResource<ResMut<T>> : std::true_type {};

/// @brief Create a tuple with only types that passes a condition
/// @tparam ...Ts
template <template <typename> class Cond, typename... Ts>
using filtered_tuple = decltype(std::tuple_cat(
    std::conditional_t<Cond<Ts>::value, std::tuple<Ts>, std::tuple<>>{}...));

struct SystemWrapper {
  SystemWrapper()
      : callback({}), c_read(0), c_write(0), r_read(0), r_write(0) {};

  SystemWrapper(std::function<void(Ecs *)> callback, bit::Bitset c_read,
                bit::Bitset c_write, bit::Bitset r_read, bit::Bitset r_write)
      : callback(callback), c_read(c_read), c_write(c_write), r_read(r_read),
        r_write(r_write) {}

  std::function<void(Ecs *)> callback;

  bit::Bitset c_read;
  bit::Bitset c_write;

  bit::Bitset r_read;
  bit::Bitset r_write;
};

struct SparseSetBase {
  virtual ~SparseSetBase() = default;

  void (*remove)(SparseSetBase *,
                 Entity); // Gets created when creating a new sparseset ->
                          // caches Type at comp time for type erased removal
};

template <typename T> struct DenseEntry {

  T component;
  Entity entity;
};

template <typename T> struct SparseSet : SparseSetBase {

  std::vector<DenseEntry<T>> dense;
  std::vector<uint32_t> sparse;
};

using removeSparseSet = void (*)(SparseSetBase *, Entity);

template <typename T> removeSparseSet makeRemoveForSparseSet() {
  return [](SparseSetBase *base, Entity e) {
    SparseSet<T> *set = static_cast<SparseSet<T> *>(base);

    uint32_t component_index = set->sparse[e];

    if (component_index == NO_ENTITY)
      return;

    Entity last_entity = set->dense.back().entity;

    set->dense[component_index] = set->dense.back();

    set->sparse[last_entity] = component_index;

    set->dense.pop_back();

    set->sparse[e] = NO_ENTITY;
  };
}

struct ResourceBase {
  virtual ~ResourceBase() = default;
};

template <typename T> struct ResourceData : ResourceBase {
  T data;
};

class Ecs {
public:
  Ecs()
      : pool(thread_pool::ThreadPool()) {

        };

  ~Ecs() {

    for (auto &set : sets) {
      delete set;
    }

    for (auto &resource : resources) {
      delete resource;
    }
  }

  struct SystemViewBase {
    virtual ~SystemViewBase() = default;
    bool is_dirty = false;
  };

  template <typename... Ts> class SystemView : SystemViewBase {

    friend class Ecs;

  public:
    SystemView(Ecs *ecs)
        : ecs(ecs)

    {
      static_assert(
          ((is_read_or_write<Ts>::value || isResource<Ts>::value) && ...),
          "All members must be in Wrappers");
    };

    template <typename T> auto &getComponent(Entity e) {
      static_assert((std::is_same_v<T, Ts> || ...),
                    "Component T is not in this system's query!");

      static_assert(
          is_read_or_write<T>::value,
          "Must be a component in Read / Write Wrapper for Multithreading");

      static SparseSet<component_t<T>> &sparse_set =
          ecs->getOrCreateSparseSet<component_t<T>>();

      if constexpr (is_read<T>::value) {

        return static_cast<const component_t<T> &>(
            sparse_set.dense[sparse_set.sparse.at(e)].component);
      } else {
        return static_cast<component_t<T> &>(
            sparse_set.dense[sparse_set.sparse.at(e)].component);
      }
    }

  private:
    Ecs *ecs;

    template <typename T> inline decltype(auto) getSystemArgument(Entity e) {
      static_assert(is_read_or_write<T>::value || isResource<T>::value,
                    "Must be a resource or component");

      if constexpr (is_read_or_write<T>::value) {
        // Is Component

        // Static so each Component SparseSet for each SystemView is only loaded
        // once, then cached
        static SparseSet<component_t<T>> &sparse_set =
            ecs->getOrCreateSparseSet<component_t<T>>();
        static std::vector<DenseEntry<component_t<T>>> *dense =
            &sparse_set.dense;
        static std::vector<uint32_t> *sparse = &sparse_set.sparse;

        if constexpr (is_read<T>::value) {
          // Assumes check for Entity happened before
          return static_cast<const component_t<T> &>(
              (*dense)[(*sparse)[e]].component);
        } else {
          return static_cast<component_t<T> &>(
              (*dense)[(*sparse)[e]].component);
        }
      } else {

        using Inner = typename unwrapResource<T>::type;

        static uint32_t resource_id = ecs->getResourceId<Inner>();

        if (resource_id >= ecs->resources.size())
          ecs->resources.resize(resource_id + 1, nullptr);

        ResourceData<Inner> *data =
            static_cast<ResourceData<Inner> *>(ecs->resources[resource_id]);

        if (data == nullptr) {
          std::cerr << "No Resource inserted\n";
          std::abort();
        }

        if constexpr (isMutableResource<T>::value) {
          return static_cast<Inner &>(data->data);
        } else {

          return static_cast<const Inner &>(data->data);
        }
      }
    }

    template <typename smallest_T> inline bool hasAllComponents(Entity e) {

      static_assert(
          ((is_read_or_write<Ts>::value || isResource<Ts>::value) && ...),
          "Must be a resource or component");

      return (... && hasComponent<Ts, smallest_T>(e));
    }

    template <typename T, typename smallest_T>
    inline bool hasComponent(Entity e) {
      if constexpr (is_read_or_write<T>::value &&
                    !std::is_same_v<component_t<T>, component_t<smallest_T>>) {
        static SparseSet<component_t<T>> &sparse_set =
            ecs->getOrCreateSparseSet<component_t<T>>();
        static std::vector<uint32_t> *sparse = &sparse_set.sparse;

        return (e < (*sparse).size() && (*sparse)[e] != NO_ENTITY);
      } else {
        return true;
      }
    }
  };

  // Static Systemhelper to avoid dependent template and get the correct
  // dependent
  template <typename T, typename... Ts>
  static inline decltype(auto) get(SystemView<Ts...> &view, Entity e) {

    using Wrapper = std::conditional_t<
        (std::is_same_v<Read<T>, Ts> || ...), Read<T>,
        std::conditional_t<(std::is_same_v<Write<T>, Ts> || ...), Write<T>,
                           void>>;

    static_assert(!std::is_void_v<Wrapper>,
                  "Type T not found in SystemView or is a Resource");

    return view.template getComponent<Wrapper>(e);
  }

  template <typename T> void addComponent(Entity e, T component) {

    if (hasComponents<T>(e))
      return;

    static_assert(!is_read_or_write<T>());

    SparseSet<T> &set = getOrCreateSparseSet<T>();

    set.dense.push_back({component, e});

    uint32_t dense_index = set.dense.size() - 1;

    if (e >= set.sparse.size()) {
      set.sparse.resize(e + 1, NO_ENTITY);
    }

    set.sparse[e] = dense_index;

    uint32_t comp_index = getTypeId<T>();

    if (e >= entity_what_components.size()) {
      entity_what_components.resize(e + 1, {});
    }

    entity_what_components[e].insert(comp_index);
  }

  template <typename T> void removeComponent(Entity e) {

    SparseSet<T> *set = getOrCreateSparseSet<T>();

    if (set == nullptr)
      return;

    uint32_t comp_index = getTypeId<T>();

    if (e >= entity_what_components.size())
      return; // Entity has not components yet

    if (entity_what_components[e].count(comp_index) == 0)
      return; // Does not have component

    entity_what_components[e].erase(comp_index);

    set->remove(set, e);
  }

  template <typename... Ts, typename Func> void forEach(Func &&func) {

    static_assert(((is_read_or_write<Ts>::value || isConstResource<Ts>::value ||
                    isMutableResource<Ts>::value) &&
                   ...),
                  "All components/resources must be wrapped in Read<T> "
                  ",Write<T>, Res<T> or ResMut<T>!");

    uint32_t dense_size_counter = 0;

    size_t dense_sizes[] = {
        (is_read_or_write<Ts>::value
             ? getOrCreateSparseSet<component_t<Ts>>().dense.size()
             : SIZE_MAX)...};

    size_t smallest_index = 0;
    size_t smallest_size = dense_sizes[0];

    auto data_tuple = std::tuple<Ts...>();

    for (size_t i = 0; i < sizeof...(Ts); i++) {
      if (dense_sizes[i] < smallest_size) {
        smallest_index = i;
        smallest_size = dense_sizes[i];
      }
    }

    size_t count = 0;

    (
        [&]() {
          if constexpr (is_read_or_write<Ts>::value) {
            if (count++ == smallest_index) {
              iterateSparseSet<Ts, Ts...>(
                  &getOrCreateSparseSet<component_t<Ts>>(), smallest_index,
                  func);
            }
          }
        }(),
        ...);
  }

  Entity createEntity() {
    static Entity e = 0;

    return e++;
  }

  template <typename T> void insertResource(T data) {
    uint32_t id = getResourceId<T>();

    if (id >= resources.size()) {
      resources.resize(id + 1, nullptr);
      resources[id] = new ResourceData<T>();
    }

    ResourceData<T> &ref = *static_cast<ResourceData<T> *>(resources[id]);

    ref.data = data;
  }

  template <typename... Ts, typename Func>
  uint32_t addSystem(Schedule &schedule, Func &&func) {

    static_assert(((is_read_or_write<Ts>::value || isConstResource<Ts>::value ||
                    isMutableResource<Ts>::value) &&
                   ...),
                  "All components must be wrapped in Read<T> or Write<T>!");

    // Unique Lookup Tables for each combination, gets only created once on
    // first call
    static const auto c_lookup_write_table = [&]() {
      bit::Bitset write(sizeof...(Ts));

      ((is_write<Ts>::value
            ? (write.setBit(getTypeId<typename unwrap_component<Ts>::type>(),
                            true),
               true)
            : false),
       ...);

      return write;
    }();

    static const auto c_lookup_read_table = [&]() {
      bit::Bitset read(sizeof...(Ts));

      ((is_read<Ts>::value
            ? (read.setBit(getTypeId<typename unwrap_component<Ts>::type>(),
                           true),
               true)
            : false),
       ...);

      return read;
    }();

    static const auto r_lookup_write_table = [&]() {
      bit::Bitset write(sizeof...(Ts));

      ((isMutableResource<Ts>::value
            ? (write.setBit(getResourceId<typename unwrapResource<Ts>::type>(),
                            true),
               true)
            : false),
       ...);

      return write;
    }();

    static const auto r_lookup_read_table = [&]() {
      bit::Bitset read(sizeof...(Ts));

      ((isConstResource<Ts>::value
            ? (read.setBit(getResourceId<typename unwrapResource<Ts>::type>(),
                           true),
               true)
            : false),
       ...);

      return read;
    }();

    std::function<void(Ecs *)> wrapper = [func](Ecs *ecs) {
      ecs->forEach<Ts...>(func);
    };

    uint32_t system_id = getNextSystemId();

    if (system_id >= systems.size()) {
      systems.resize(system_id + 1);
    }

    SystemWrapper system(wrapper, c_lookup_read_table, c_lookup_write_table,
                         r_lookup_read_table, r_lookup_write_table);

    systems[system_id] = system;

    schedule.systems.insert(system_id);

    return system_id;
  }

  void removeSystem(Schedule &schedule, uint32_t system_id) {
    schedule.systems.erase(system_id);
  }

  void runSchedule(Schedule schedule) {

    std::vector<uint32_t> system_ids(schedule.systems.begin(),
                                     schedule.systems.end());

    for (uint32_t system_id : schedule.systems) {
      SystemWrapper &current = systems[system_id];

      current.callback(this);
    }
  }

  void runScheduleParallel(Schedule schedule) {

    auto checkConflict = [&schedule](const SystemWrapper &a,
                                     const SystemWrapper &b) {
      bool c_conflict =
          ((a.c_write & b.c_write).any() || (a.c_write & b.c_read).any() ||
           (b.c_write & a.c_read).any());
      bool r_conflict =
          ((a.r_write & b.r_write).any() || (a.r_write & b.r_read).any() ||
           (b.r_write & a.r_read).any());

      return (c_conflict || r_conflict);
    };

    std::vector<uint32_t> system_ids(schedule.systems.begin(),
                                     schedule.systems.end());

    std::vector<std::vector<SystemWrapper *>> batches = {};

    for (uint32_t system_id : system_ids) {
      SystemWrapper &current = systems[system_id];
      bool added_to_batch = false;

      for (auto &batch : batches) {
        bool conflict = false;

        for (SystemWrapper *existing : batch) {
          if (checkConflict(current, *existing)) {
            conflict = true;
            break;
          }
        }

        if (!conflict) {
          batch.push_back(&current);
          added_to_batch = true;
          break;
        }
      }

      // Need more place, no place in other batches
      if (!added_to_batch) {
        batches.push_back({&current});
      }
    }

    for (auto &batch : batches) {

      std::atomic<size_t> jobs_remaining = batch.size();
      std::condition_variable cv;
      std::mutex cvMutex;

      for (auto *sys : batch) {

        pool.enqueue([this, sys, &jobs_remaining, &cv]() {
          sys->callback(this);

          if (--jobs_remaining == 0)
            cv.notify_one(); // wake up main thread when all jobs done
        });
      }

      {
        std::unique_lock<std::mutex> lock(cvMutex);
        cv.wait(lock, [&]() { return jobs_remaining == 0; });
      }
    }
  }

  void removeEntity(Entity e) {

    if (e >= entity_what_components.size())
      return; // Has no components or does not exist

    for (size_t i = 0; i < entity_what_components[e].size(); i++) {
      SparseSetBase *set = sets[i];

      set->remove(set, e);
    }

    entity_what_components[e].clear();
  };

  template <typename T> T *getComponent(Entity e) {
    SparseSet<T> set = getOrCreateSparseSet<T>();

    if (e >= set.sparse.size() || set.sparse[e] == NO_ENTITY)
      return nullptr;

    return &set.dense[set.sparse[e]].component;
  }

  template <typename T> T *getResource() {

    uint32_t id = getResourceId<T>();

    if (id >= resources.size()) {
      return nullptr;
    }

    ResourceData<T> *resource = static_cast<ResourceData<T> *>(resources[id]);

    return &resource->data;
  }

private:
  thread_pool::ThreadPool pool;

  template <typename T> resource_r<T> getResourceForLoop() {
    return getResource<typename unwrapResource<T>::type>();
  }

  template <typename T> lambda_t<T> getComponentForLoop(Entity e) {
    return *getComponent<component_t<T>>(e);

    // Lambda ensures read write access
  }

  template <typename... Ts> bool hasComponents(Entity e) {

    return (... && (checkIfEntityHasComponent<Ts>(e)));
  }

  template <typename T> bool checkIfEntityHasComponent(Entity e) {
    if constexpr (isConstResource<T>::value || isMutableResource<T>::value) {
      // Resources are global and return always true
      return true;
    } else {
      return e < getOrCreateSparseSet<component_t<T>>().sparse.size() &&
             getOrCreateSparseSet<component_t<T>>().sparse[e] != NO_ENTITY;
    }
  }

  template <typename T>
  auto getComponentOrResourceForLoop(Entity e) -> decltype(auto) {
    if constexpr (is_read_or_write<T>::value)
      return getComponentForLoop<T>(e);

    else
      return getResourceForLoop<T>();
  }

  template <typename smallest_T, typename... Ts, typename Func>
  inline void iterateSparseSet(SparseSet<component_t<smallest_T>> *smallest_set,
                               uint32_t smallest_index, Func &&func) noexcept {

    // smallest T is still in Wrapper

    // Ensure that it is wrapped in either Component or Resource Wrapper
    static_assert(is_read_or_write<smallest_T>::value);
    static_assert(
        ((is_read_or_write<Ts>::value || isMutableResource<Ts>::value ||
          isConstResource<Ts>::value) &&
         ...));

    if (smallest_set == nullptr)
      return;

    SystemView<Ts...> view(this);

    size_t smallest_size = smallest_set->dense.size();
    for (size_t i = 0; i < smallest_size; i++) {
      Entity e = smallest_set->dense[i].entity;

      if (!view.template hasAllComponents<smallest_T>(e))
        continue;

      func(view, e, view.template getSystemArgument<Ts>(e)...);
    }
  }

  template <typename T> SparseSet<T> &getOrCreateSparseSet() {
    uint32_t type_id = getTypeId<T>();

    if (type_id >= sets.size())
      sets.resize(type_id + 1, nullptr);

    if (sets[type_id] == nullptr) {
      sets[type_id] = new SparseSet<T>();
      sets[type_id]->remove = makeRemoveForSparseSet<T>();
    }

    return *static_cast<SparseSet<T> *>(sets[type_id]);
  }

  template <typename T> inline static uint32_t getTypeId() noexcept {
    static const uint32_t id = next_id++;
    return id;
  }

  static inline uint32_t next_id = 0;

  std::vector<SparseSetBase *> sets = {};

  template <typename T> uint32_t getResourceId() {
    static uint32_t id =
        next_resource_id++; // Static = Unique per Resource Type
    return id;
  }

  static inline uint32_t next_resource_id = 0;

  std::vector<ResourceBase *> resources = {};

  inline uint32_t getNextSystemId() {
    static uint32_t next_system_id = 0;
    uint32_t id = next_system_id++;
    return id;
  };

  std::vector<SystemWrapper> systems;

  std::vector<std::unordered_set<uint32_t>>
      entity_what_components; // Caches what entity has which components
};
} // namespace vecs