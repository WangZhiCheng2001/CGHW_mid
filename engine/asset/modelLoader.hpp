#pragma once

#include <rapidobj.hpp>
#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/hash.hpp>

#include <boundingBox.hpp>

inline auto loadModel(const std::filesystem::path &filePath)
{
    auto data = rapidobj::ParseFile(filePath);
    if (data.error)
    {
        spdlog::error(data.error.code.message());
        abort();
        exit(-1);
    }
    if (!rapidobj::Triangulate(data))
    {
        spdlog::error("Loaded model [{}] is failed to triangulate.", filePath.generic_string());
        abort();
        exit(-1);
    }

    // post--only read vertex position and use flat normal
    std::vector<glm::vec4> vertices;
    std::vector<uint32_t> indices;
    BoundingBox box;

    // parallel read
    std::vector<std::vector<glm::vec4>> partial_vertices(data.shapes.size());
    std::vector<std::vector<uint32_t>> partial_indices(data.shapes.size());
    std::vector<BoundingBox> partial_box(data.shapes.size());
    struct sMeshTask
    {
        const rapidobj::Result *loadedData;
        uint32_t shapeIndex;
    };
    std::vector<sMeshTask> task(data.shapes.size());
    for (auto i = 0; i < data.shapes.size(); ++i)
    {
        task[i].loadedData = &data;
        task[i].shapeIndex = i;
    }

    auto concurrency = std::min(static_cast<size_t>(std::thread::hardware_concurrency()), task.size());
    auto threads = std::vector<std::thread>{};
    threads.reserve(concurrency);

    auto taskIndex = std::atomic_size_t{0ULL};
    auto activeThreads = std::atomic_size_t{concurrency};
    auto completed = std::promise<void>{};

    auto workerFunc = [&]()
    {
        auto activeTaskIndex = std::atomic_fetch_add(&taskIndex, 1ULL);
        while (activeTaskIndex < task.size())
        {
            {
                // avoid vertex duplicate
                std::unordered_map<glm::vec4, uint32_t> verticesMap;

                auto attributes = &task[activeTaskIndex].loadedData->attributes;
                auto shape = &task[activeTaskIndex].loadedData->shapes[task[activeTaskIndex].shapeIndex];
                auto &vertices = partial_vertices[task[activeTaskIndex].shapeIndex];
                auto &indices = partial_indices[task[activeTaskIndex].shapeIndex];
                auto &box = partial_box[task[activeTaskIndex].shapeIndex];
                vertices.reserve(shape->mesh.indices.size());
                indices.reserve(shape->mesh.indices.size());
                for (const auto &index : shape->mesh.indices)
                {
                    glm::vec4 pos = {
                        attributes->positions[3 * index.position_index + 0],
                        attributes->positions[3 * index.position_index + 1],
                        attributes->positions[3 * index.position_index + 2],
                        1.0f};

                    if (verticesMap.count(pos) == 0)
                    {
                        verticesMap[pos] = vertices.size();
                        vertices.emplace_back(pos);
                        if (box.minPoint == glm::zero<glm::vec3>() && box.maxPoint == glm::zero<glm::vec3>())
                            box.minPoint = box.maxPoint = pos;
                        else
                            box.extend(pos);
                    }
                    indices.emplace_back(verticesMap[pos]);
                }
                vertices.shrink_to_fit();
                indices.shrink_to_fit();
            }
            activeTaskIndex = std::atomic_fetch_add(&taskIndex, 1ULL);
        }

        if (std::atomic_fetch_sub(&activeThreads, 1ULL) == 1)
            completed.set_value();
    };

    for (auto i = 0; i < concurrency; ++i)
    {
        threads.emplace_back(workerFunc);
        threads.back().detach();
    }

    completed.get_future().wait();

    // merge results
    auto offset = 0ULL;
    for (auto i = 0; i < partial_vertices.size(); ++i)
    {
        vertices.insert(vertices.end(), partial_vertices[i].begin(), partial_vertices[i].end());
        auto beginIndex = indices.size();
        indices.insert(indices.end(), partial_indices[i].begin(), partial_indices[i].end());
        std::for_each(indices.begin() + beginIndex, indices.end(), [&](uint32_t &elem)
                      { elem += offset; });
        offset = vertices.size();
        if (i == 0)
            box = partial_box[i];
        else
            box.extend(partial_box[i]);
    }

    return std::tuple{vertices, indices, box};
}