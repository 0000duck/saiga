//
// Created by Peter Eichinger on 2019-01-21.
//

#include "Defragger.h"

#include <saiga/util/threadName.h>

using namespace Saiga::Vulkan::Memory;
static std::atomic_int counter = 0;
void Defragger::start()
{
    if (!enabled)
    {
        return;
    }
    if (running)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(start_mutex);
        running = true;
    }
    start_condition.notify_one();
}

void Defragger::stop()
{
    running = false;
    std::unique_lock<std::mutex> lock(running_mutex);
}



void Defragger::worker_func()
{
    Saiga::setThreadName("Defragger " + std::to_string(counter++));
    while (true)
    {
        std::unique_lock<std::mutex> lock(start_mutex);
        start_condition.wait(lock, [this] { return running || quit; });
        if (quit)
        {
            return;
        }
        std::unique_lock<std::mutex> running_lock(running_mutex);
        if (chunks->empty())
        {
            continue;
        }

        apply_invalidations();

        perform_defragmentation();
    }
}

void Defragger::perform_defragmentation()
{
    auto chunk_iter = chunks->rbegin();
    auto alloc_iter = chunk_iter->allocations.rbegin();
    while (running)
    {
        if (alloc_iter == chunk_iter->allocations.rend())
        {
            ++chunk_iter;
            alloc_iter = chunk_iter->allocations.rbegin();
            continue;
        }

        if (chunk_iter == chunks->rend())
        {
            running = false;
            break;
        }

        const auto& source = *alloc_iter;

        auto begin = chunks->begin();
        auto end   = (chunk_iter + 1).base();  // Conversion from reverse to normal iterator moves one back
        //
        auto new_place = strategy->findRange(begin, end, source.size);

        if (new_place.first != end)
        {
            const auto target_iter = new_place.second;
            const auto& target     = *target_iter;
            auto weight            = get_operation_penalty(new_place.first, target_iter, end, (alloc_iter + 1).base());
            LOG(INFO) << "Defrag " << source << "->" << target << " :: " << weight;

            defrag_operations.insert(DefragOperation{source, target, weight});
        }
        alloc_iter++;
    }
}

void Defragger::invalidate(vk::DeviceMemory memory)
{
    std::unique_lock<std::mutex> invalidate_lock(invalidate_mutex);
    invalidate_set.insert(memory);
}

float Defragger::get_operation_penalty(ConstChunkIterator target_chunk, ConstLocationIterator target_location,
                                       ConstChunkIterator source_chunk, ConstLocationIterator source_location) const
{
    float weight = 0;
    // if the move creates a hole that is smaller than the memory chunk itself -> add weight
    if (target_location->size != source_location->size &&
        (target_location->size - source_location->size < source_location->size))
    {
        weight +=
            penalties.target_small_hole * (1 - (static_cast<float>(source_location->size) / target_location->size));
    }

    // If move creates a hole at source -> add weight
    auto next = std::next(source_location);
    if (source_location != source_chunk->allocations.cbegin() && next != source_chunk->allocations.cend())
    {
        auto prev = std::prev(source_location);

        if (source_location->offset == prev->offset + prev->size &&
            source_location->offset + source_location->size == next->offset)
        {
            weight += penalties.source_create_hole;
        }
    }

    // Penalty if allocation is not the last allocation in chunk
    if (next != source_chunk->allocations.cend())
    {
        weight += penalties.source_not_last_alloc;
    }

    if (std::next(source_chunk) != chunks->cend())
    {
        weight += penalties.source_not_last_chunk;
    }

    return weight;
}

void Defragger::apply_invalidations()
{
    std::unique_lock<std::mutex> invalidate_lock(invalidate_mutex);
    if (!defrag_operations.empty() && !invalidate_set.empty())
    {
        auto ops_iter = defrag_operations.begin();

        while (ops_iter != defrag_operations.end())
        {
            auto target_mem = ops_iter->target.memory;
            if (invalidate_set.find(target_mem) != invalidate_set.end())
            {
                ops_iter = defrag_operations.erase(ops_iter);
            }
            else
            {
                ++ops_iter;
            }
        }
        invalidate_set.clear();
    }
}
