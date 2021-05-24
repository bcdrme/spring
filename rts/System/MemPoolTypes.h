/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef MEMPOOL_TYPES_H
#define MEMPOOL_TYPES_H

#include <cassert>
#include <cstring> // memset

#include <array>
#include <deque>
#include <vector>
#include <map>
#include <memory>

#include "System/UnorderedMap.hpp"
#include "System/ContainerUtil.h"
#include "System/SafeUtil.h"
#include "System/Platform/Threading.h"
#include "System/Threading/SpringThreading.h"
#include "System/Log/ILog.h"

template<size_t S> struct DynMemPool {
public:
	void* allocMem(size_t size) {
		assert(size <= PAGE_SIZE());
		uint8_t* m = nullptr;

		size_t i = 0;

		if (indcs.empty()) {
			pages.emplace_back();

			i = pages.size() - 1;
		} else {
			// must pop before ctor runs; objects can be created recursively
			i = spring::VectorBackPop(indcs);
		}

		m = pages[curr_page_index = i].data();

		table.emplace(m, i);
		return m;
	}


	template<typename T, typename... A> T* alloc(A&&... a) {
		static_assert(sizeof(T) <= PAGE_SIZE(), "");
		return new (allocMem(sizeof(T))) T(std::forward<A>(a)...);
	}


	void freeMem(void* m) {
		assert(mapped(m));

		const auto iter = table.find(m);
		const auto pair = std::pair<void*, size_t>{iter->first, iter->second};

		std::memset(pages[pair.second].data(), 0, PAGE_SIZE());

		indcs.push_back(pair.second);
		table.erase(pair.first);
	}


	template<typename T> void free(T*& p) {
		assert(mapped(p));
		void* m = p;

		spring::SafeDestruct(p);
		// must free after dtor runs, since that can trigger *another* ctor call
		// by proxy (~CUnit -> ~CObject -> DependentDied -> CommandAI::FinishCmd
		// -> CBuilderCAI::ExecBuildCmd -> UnitLoader::LoadUnit -> CUnit e.g.)
		freeMem(m);
	}

	static constexpr size_t PAGE_SIZE() { return S; }

	size_t alloc_size() const { return (pages.size() * PAGE_SIZE()); } // size of total number of pages added over the pool's lifetime
	size_t freed_size() const { return (indcs.size() * PAGE_SIZE()); } // size of number of pages that were freed and are awaiting reuse

	bool mapped(void* p) const { return (table.find(p) != table.end()); }
	bool alloced(void* p) const { return ((curr_page_index < pages.size()) && (pages[curr_page_index].data() == p)); }

	void clear() {
		pages.clear();
		indcs.clear();
		table.clear();

		curr_page_index = 0;
	}
	void reserve(size_t n) {
		indcs.reserve(n);
		table.reserve(n);
	}

private:
	std::deque<std::array<uint8_t, S>> pages;
	std::vector<size_t> indcs;

	// <pointer, page index> (non-intrusive)
	spring::unsynced_map<void*, size_t> table;

	size_t curr_page_index = 0;
};



// fixed-size dynamic version
// page size per chunk, number of chunks, number of pages per chunk
// at most <N * K> simultaneous allocations can be made from a pool
// of size NxK, each of which consumes S bytes (N chunks with every
// chunk consuming S * K bytes) excluding overhead
template<size_t S, size_t N, size_t K> struct FixedDynMemPool {
public:
	template<typename T, typename... A> T* alloc(A&&... a) {
		static_assert(sizeof(T) <= PAGE_SIZE(), "");
		return (new (allocMem(sizeof(T))) T(std::forward<A>(a)...));
	}

	void* allocMem(size_t size) {
		uint8_t* ptr = nullptr;

		if (indcs.empty()) {
			// pool is full
			if (num_chunks == N)
				return ptr;

			assert(chunks[num_chunks] == nullptr);
			chunks[num_chunks].reset(new t_chunk_mem());

			// reserve new indices; in reverse order since each will be popped from the back
			indcs.reserve(K);

			for (size_t j = 0; j < K; j++) {
				indcs.push_back(static_cast<uint32_t>((num_chunks + 1) * K - j - 1));
			}

			num_chunks += 1;
		}

		const uint32_t idx = spring::VectorBackPop(indcs);

		assert(size <= PAGE_SIZE());
		memcpy(ptr = page_mem(page_index = idx), &idx, sizeof(idx));
		return (ptr + sizeof(idx));
	}


	template<typename T> void free(T*& ptr) {
		static_assert(sizeof(T) <= PAGE_SIZE(), "");

		T* tmp = ptr;

		spring::SafeDestruct(ptr);
		freeMem(tmp);
	}

	void freeMem(void* ptr) {
		const uint32_t idx = page_idx(ptr);

		// zero-fill page
		assert(idx < (N * K));
		memset(page_mem(idx), 0, sizeof(idx) + S);

		indcs.push_back(idx);
	}


	void reserve(size_t n) { indcs.reserve(n); }
	void clear() {
		indcs.clear();

		// for every allocated chunk, add back all indices
		// (objects are assumed to have already been freed)
		for (size_t i = 0; i < num_chunks; i++) {
			for (size_t j = 0; j < K; j++) {
				indcs.push_back(static_cast<uint32_t>((i + 1) * K - j - 1));
			}
		}

		page_index = 0;
	}


	static constexpr size_t NUM_CHUNKS() { return N; } // size K*S
	static constexpr size_t NUM_PAGES() { return K; } // per chunk
	static constexpr size_t PAGE_SIZE() { return S; }

	const uint8_t* page_mem(size_t idx, size_t ofs = 0) const {
		const t_chunk_ptr& chunk_ptr = chunks[idx / K];
		const t_chunk_mem& chunk_mem = *chunk_ptr;
		return (&chunk_mem[idx % K][0] + ofs);
	}
	uint8_t* page_mem(size_t idx, size_t ofs = 0) {
		t_chunk_ptr& chunk_ptr = chunks[idx / K];
		t_chunk_mem& chunk_mem = *chunk_ptr;
		return (&chunk_mem[idx % K][0] + ofs);
	}

	uint32_t page_idx(void* ptr) const {
		const uint8_t* raw_ptr = reinterpret_cast<const uint8_t*>(ptr);
		const uint8_t* idx_ptr = raw_ptr - sizeof(uint32_t);

		return (*reinterpret_cast<const uint32_t*>(idx_ptr));
	}

	size_t alloc_size() const { return (num_chunks * NUM_PAGES() * PAGE_SIZE()); } // size of total number of pages added over the pool's lifetime
	size_t freed_size() const { return (indcs.size() * PAGE_SIZE()); } // size of number of pages that were freed and are awaiting reuse

	bool mapped(void* ptr) const { return ((page_idx(ptr) < (num_chunks * K)) && (page_mem(page_idx(ptr), sizeof(uint32_t)) == ptr)); }
	bool alloced(void* ptr) const { return ((page_index < (num_chunks * K)) && (page_mem(page_index, sizeof(uint32_t)) == ptr)); }

private:
	// first sizeof(uint32_t) bytes are reserved for index
	typedef std::array<uint8_t[sizeof(uint32_t) + S], K> t_chunk_mem;
	typedef std::unique_ptr<t_chunk_mem> t_chunk_ptr;

	std::array<t_chunk_ptr, N> chunks;
	std::vector<uint32_t> indcs;

	size_t num_chunks = 0;
	size_t page_index = 0;
};



// fixed-size version
template<size_t N, size_t S> struct StaticMemPool {
public:
	StaticMemPool() { clear(); }

	void* allocMem(size_t size) {
		assert(size <= PAGE_SIZE());
		static_assert(NUM_PAGES() != 0, "");

		size_t i = 0;

		assert(can_alloc());

		if (free_page_count == 0) {
			i = used_page_count++;
		} else {
			i = indcs[--free_page_count];
		}

		return (pages[curr_page_index = i].data());
	}


	template<typename T, typename... A> T* alloc(A&&... a) {
		static_assert(sizeof(T) <= PAGE_SIZE(), "");
		return new (allocMem(sizeof(T))) T(std::forward<A>(a)...);
	}

	void freeMem(void* m) {
		assert(can_free());
		assert(mapped(m));

		std::memset(m, 0, PAGE_SIZE());

		// mark page as free
		indcs[free_page_count++] = base_offset(m) / PAGE_SIZE();
	}


	template<typename T> void free(T*& p) {
		assert(mapped(p));
		void* m = p;

		spring::SafeDestruct(p);
		freeMem(m);
	}


	static constexpr size_t NUM_PAGES() { return N; }
	static constexpr size_t PAGE_SIZE() { return S; }

	size_t alloc_size() const { return (used_page_count * PAGE_SIZE()); } // size of total number of pages added over the pool's lifetime
	size_t freed_size() const { return (free_page_count * PAGE_SIZE()); } // size of number of pages that were freed and are awaiting reuse
	size_t total_size() const { return (NUM_PAGES() * PAGE_SIZE()); }
	size_t base_offset(const void* p) const { return (reinterpret_cast<const uint8_t*>(p) - reinterpret_cast<const uint8_t*>(pages[0].data())); }

	bool mapped(const void* p) const { return (((base_offset(p) / PAGE_SIZE()) < total_size()) && ((base_offset(p) % PAGE_SIZE()) == 0)); }
	bool alloced(const void* p) const { return (pages[curr_page_index].data() == p); }

	bool can_alloc() const { return (used_page_count < NUM_PAGES() || free_page_count > 0); }
	bool can_free() const { return (free_page_count < NUM_PAGES()); }

	void reserve(size_t) {} // no-op
	void clear() {
		std::memset(pages.data(), 0, total_size());
		std::memset(indcs.data(), 0, NUM_PAGES());

		used_page_count = 0;
		free_page_count = 0;
		curr_page_index = 0;
	}

private:
	std::array<std::array<uint8_t, S>, N> pages;
	std::array<size_t, N> indcs;

	size_t used_page_count = 0;
	size_t free_page_count = 0; // indcs[fpc-1] is the last recycled page
	size_t curr_page_index = 0;
};


// dynamic memory allocator operating with stable index positions
// has gaps management
template <typename T>
class StablePosAllocator {
public:
	static constexpr bool reportWork = false;
	template<typename ...Args>
	static void myLog(Args&&... args) {
		if (!reportWork)
			return;
		LOG(std::forward<Args>(args)...);
	}
public:
	StablePosAllocator() = default;
	StablePosAllocator(size_t initialSize) :StablePosAllocator() {
		data.reserve(initialSize);
	}
	void Reset() {
		CompactGaps();
		//upon compaction all allocations should go away
		assert(data.empty());
		assert(sizeToPositions.empty());
		assert(positionToSize.empty());
	}

	size_t Allocate(size_t numElems, bool withMutex = false);
	void Free(size_t& firstElem, size_t numElems);
	const size_t GetSize() const { return data.size(); }
	std::vector<T>& GetData() { return data; }

	T& operator[](std::size_t idx) { return data[idx]; }
	const T& operator[](std::size_t idx) const { return data[idx]; }
private:
	void CompactGaps();
	size_t AllocateImpl(size_t numElems);
private:
	spring::mutex mut;
	std::vector<T> data;
	std::multimap<size_t, size_t> sizeToPositions;
	std::map<size_t, size_t> positionToSize;
};


template<typename T>
inline size_t StablePosAllocator<T>::Allocate(size_t numElems, bool withMutex)
{
	if (withMutex) {
		std::lock_guard<spring::mutex> lck(mut);
		return AllocateImpl(numElems);
	}
	else
		return AllocateImpl(numElems);
}

template<typename T>
inline size_t StablePosAllocator<T>::AllocateImpl(size_t numElems)
{
	if (numElems == 0)
		return ~0u;

	//no gaps
	if (positionToSize.empty()) {
		size_t returnPos = data.size();
		data.resize(data.size() + numElems);
		myLog("StablePosAllocator<T>::AllocateImpl(%u) = %u [thread_id = %u]", uint32_t(numElems), uint32_t(returnPos), static_cast<uint32_t>(Threading::GetCurrentThreadId()));
		return returnPos;
	}

	//try to find gaps >= in size than requested
	for (auto it = sizeToPositions.lower_bound(numElems); it != sizeToPositions.end(); ++it) {
		if (it->first < numElems)
			continue;

		size_t returnPos = it->second;
		positionToSize.erase(it->second);

		if (it->first > numElems) {
			size_t gapSize = it->first - numElems;
			size_t gapPos = it->second + numElems;
			sizeToPositions.emplace(gapSize, gapPos);
			positionToSize.emplace(gapPos, gapSize);
		}

		sizeToPositions.erase(it);
		myLog("StablePosAllocator<T>::AllocateImpl(%u) = %u", uint32_t(numElems), uint32_t(returnPos));
		return returnPos;
	}

	//all gaps are too small
	size_t returnPos = data.size();
	data.resize(data.size() + numElems);
	myLog("StablePosAllocator<T>::AllocateImpl(%u) = %u", uint32_t(numElems), uint32_t(returnPos));
	return returnPos;
}

//merge adjacent gaps and trim data vec
template<typename T>
inline void StablePosAllocator<T>::CompactGaps()
{
	//helper to erase {size, pos} pair from sizeToPositions multimap
	const auto eraseSizeToPositionsKVFunc = [this](size_t size, size_t pos) {
		auto [beg, end] = sizeToPositions.equal_range(size);
		for (auto it = beg; it != end; /*noop*/)
			if (it->second == pos) {
				it = sizeToPositions.erase(it);
				break;
			}
			else {
				++it;
			}
	};

	bool found;
	std::size_t posStartFrom = 0u;
	do {
		found = false;

		std::map<size_t, size_t>::iterator posSizeBeg = positionToSize.lower_bound(posStartFrom);
		std::map<size_t, size_t>::iterator posSizeFin = positionToSize.end(); std::advance(posSizeFin, -1);

		for (auto posSizeThis = posSizeBeg; posSizeThis != posSizeFin; ++posSizeThis) {
			posStartFrom = posSizeThis->first;
			auto posSizeNext = posSizeThis; std::advance(posSizeNext, 1);

			if (posSizeThis->first + posSizeThis->second == posSizeNext->first) {
				std::size_t newPos = posSizeThis->first;
				std::size_t newSize = posSizeThis->second + posSizeNext->second;

				eraseSizeToPositionsKVFunc(posSizeThis->second, posSizeThis->first);
				eraseSizeToPositionsKVFunc(posSizeNext->second, posSizeNext->first);

				positionToSize.erase(posSizeThis);
				positionToSize.erase(posSizeNext); //this iterator is guaranteed to stay valid after 1st erase

				positionToSize.emplace(newPos, newSize);
				sizeToPositions.emplace(newSize, newPos);

				found = true;

				break;
			}
		}
	} while (found);

	std::map<size_t, size_t>::iterator posSizeFin = positionToSize.end(); std::advance(posSizeFin, -1);
	if (posSizeFin->first + posSizeFin->second == data.size()) {
		//trim data vector
		data.resize(posSizeFin->first);
		//erase old sizeToPositions
		eraseSizeToPositionsKVFunc(posSizeFin->second, posSizeFin->first);
		//erase old positionToSize
		positionToSize.erase(posSizeFin);
	}
}

template<typename T>
inline void StablePosAllocator<T>::Free(size_t& firstElem, size_t numElems)
{
	assert(firstElem + numElems <= data.size());

	if (numElems == 0) {
		myLog("StablePosAllocator<T>::Free(%u, %u)", uint32_t(firstElem), uint32_t(numElems));
		firstElem = ~0u;
		return;
	}

	//lucky us, just remove trim the vector size
	if (firstElem + numElems == data.size()) {
		myLog("StablePosAllocator<T>::Free(%u, %u)", uint32_t(firstElem), uint32_t(numElems));
		data.resize(firstElem);
		firstElem = ~0u;
		return;
	}

	positionToSize.emplace(firstElem, numElems);
	sizeToPositions.emplace(numElems, firstElem);

	static constexpr float compactionTriggerFraction = 0.025f;
	if (positionToSize.size() >= std::ceil(compactionTriggerFraction * data.size()))
		CompactGaps();

	myLog("StablePosAllocator<T>::Free(%u, %u)", uint32_t(firstElem), uint32_t(numElems));
	firstElem = ~0u;
}

#endif

