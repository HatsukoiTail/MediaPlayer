#include "Slic.h"

#include <cassert>
#include <cmath>

#include <omp.h>

struct ClusterCenter
{
    float l;
    float a;
    float b;
    int x;
    int y;
};

// 计算某一点的梯度
static float compute_gradient(std::span<const float> lab_pixels, int width, int height, int x, int y)
{
    if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1)
    {
        return std::numeric_limits<float>::max();
    }

    int idx = (y * width + x) * 3;
    float l = lab_pixels[idx];

    float l_left = lab_pixels[((y * width + (x - 1)) * 3)];
    float l_right = lab_pixels[((y * width + (x + 1)) * 3)];
    float l_up = lab_pixels[(((y - 1) * width + x) * 3)];
    float l_down = lab_pixels[(((y + 1) * width + x) * 3)];

    float grad_x = std::abs(l_right - l_left);
    float grad_y = std::abs(l_down - l_up);

    return grad_x + grad_y;
}

// 初始化聚类中心
static std::vector<ClusterCenter> init_clusters(std::span<const float> lab_pixels, int width, int height, int scale)
{
    const int super_width = std::ceil(width / static_cast<float>(scale));
    const int super_height = std::ceil(height / static_cast<float>(scale));

    std::vector<ClusterCenter> centers(super_width * super_height);

#pragma omp parallel for
    for (size_t k = 0; k < centers.size(); k++)
    {
        // 超像素网格坐标
        int i = k % super_width;
        int j = k / super_width;

        // 网格中心坐标
        int x = i * scale + scale / 2;
        int y = j * scale + scale / 2;
        // 确保中心坐标在图像范围内
        x = std::min(x, width - 1);
        y = std::min(y, height - 1);

        int best_x = x;
        int best_y = y;
        float min_gradient = compute_gradient(lab_pixels, width, height, x, y);

        // 在3x3邻域内寻找梯度最小的点作为聚类中心
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                int nx = x + dx;
                int ny = y + dy;

                float gradient = compute_gradient(lab_pixels, width, height, nx, ny);
                if (gradient < min_gradient)
                {
                    min_gradient = gradient;
                    best_x = nx;
                    best_y = ny;
                }
            }
        }

        int idx = (best_y * width + best_x) * 3;
        centers[k] = ClusterCenter{
            lab_pixels[idx],
            lab_pixels[idx + 1],
            lab_pixels[idx + 2],
            best_x,
            best_y};
    }

    return centers;
}

static std::vector<int> init_labels(int width, int height, int scale)
{
    auto labels = std::vector<int>(width * height, 0);
    const int super_width = std::ceil(width / static_cast<float>(scale));

#pragma omp parallel for
    for (int idx = 0; idx < width * height; idx++)
    {
        int x = idx % width;
        int y = idx / width;

        int grid_x = static_cast<int>(x / scale);
        int grid_y = static_cast<int>(y / scale);

        labels[idx] = grid_y * super_width + grid_x;
    }
    return labels;
}

static std::vector<float> init_distances(int width, int height)
{
    return std::vector<float>(width * height, std::numeric_limits<float>::max());
}

static void update_cluster_centers(std::vector<ClusterCenter> &cluster_centers, const std::span<const float> lab_pixels, const std::span<const int> &labels, int width, int height)
{
    int num_thread = omp_get_max_threads();
    std::vector<std::vector<ClusterCenter>> thread_sums(num_thread, std::vector<ClusterCenter>(cluster_centers.size(), ClusterCenter{0, 0, 0, 0, 0}));
    std::vector<std::vector<int>> thread_counts(num_thread, std::vector<int>(cluster_centers.size(), 0));

#pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        auto &local_sum = thread_sums[thread_id];
        auto &local_counts = thread_counts[thread_id];

#pragma omp for
        for (size_t idx = 0; idx < labels.size(); idx++)
        {
            int cluster_id = labels[idx];

            if (cluster_id < 0 || cluster_id >= static_cast<int>(cluster_centers.size()))
                continue;

            int pixel_idx = idx * 3;

            local_sum[cluster_id].l += lab_pixels[pixel_idx];
            local_sum[cluster_id].a += lab_pixels[pixel_idx + 1];
            local_sum[cluster_id].b += lab_pixels[pixel_idx + 2];
            local_sum[cluster_id].x += (idx % width);
            local_sum[cluster_id].y += (idx / width);

            local_counts[cluster_id]++;
        }
    }

    // 汇总各线程的结果
    for (int i = 0; i < static_cast<int>(cluster_centers.size()); i++)
    {
        int total_count = 0;
        for (int t = 0; t < num_thread; t++)
        {
            cluster_centers[i].l += thread_sums[t][i].l;
            cluster_centers[i].a += thread_sums[t][i].a;
            cluster_centers[i].b += thread_sums[t][i].b;
            cluster_centers[i].x += thread_sums[t][i].x;
            cluster_centers[i].y += thread_sums[t][i].y;

            total_count += thread_counts[t][i];
        }

        if (total_count > 0)
        {
            cluster_centers[i].l /= total_count;
            cluster_centers[i].a /= total_count;
            cluster_centers[i].b /= total_count;
            cluster_centers[i].x /= total_count;
            cluster_centers[i].y /= total_count;
        }
    }
}

static void update_distance(std::vector<float> &distances, std::vector<int> &labels, std::span<const float> lab_pixels, const std::vector<ClusterCenter> &centers, int width, int height, int scale)
{
    std::fill(distances.begin(), distances.end(), std::numeric_limits<float>::max());

    const int num_clusters = centers.size();
    const int super_width = std::ceil(width / static_cast<float>(scale));
    const int super_height = std::ceil(height / static_cast<float>(scale));

    // 动态计算权重 m/S
    const float m = 10.0f; // 紧凑度参数
    const float m_over_s = m / scale;
    const float spatial_weight = m_over_s * m_over_s;

#pragma omp parallel for
    for (int idx = 0; idx < num_clusters; idx++)
    {
        const ClusterCenter &center = centers[idx];

        // 计算中心所在的网格坐标
        int grid_x = static_cast<int>(center.x / scale);
        int grid_y = static_cast<int>(center.y / scale);

        // 搜索周围 3x3 的网格区域（覆盖 2S 半径）
        for (int gy = std::max(0, grid_y - 1); gy <= std::min(super_height - 1, grid_y + 1); ++gy)
        {
            for (int gx = std::max(0, grid_x - 1); gx <= std::min(super_width - 1, grid_x + 1); ++gx)
            {
                int k = gy * super_width + gx;
                if (k >= num_clusters)
                    continue; // 防止越界

                // 遍历该网格内的像素
                int y_start = gy * scale;
                int y_end = std::min((gy + 1) * scale, height);
                int x_start = gx * scale;
                int x_end = std::min((gx + 1) * scale, width);

                for (int y = y_start; y < y_end; y++)
                {
                    for (int x = x_start; x < x_end; x++)
                    {
                        int pixel_idx = (y * width + x) * 3;
                        float dl = lab_pixels[pixel_idx] - center.l;
                        float da = lab_pixels[pixel_idx + 1] - center.a;
                        float db = lab_pixels[pixel_idx + 2] - center.b;

                        float color_dist_sq = dl * dl + da * da + db * db;

                        float dx = x - center.x;
                        float dy = y - center.y;
                        float spatial_dist_sq = dx * dx + dy * dy;

                        float distance = color_dist_sq + spatial_weight * spatial_dist_sq;

                        int idx_1d = y * width + x;

                        // 简单的竞争处理：如果距离更小则更新
                        // 在高并发下可能不严谨
                        if (distance < distances[idx_1d])
                        {
                            distances[idx_1d] = distance;
                            labels[idx_1d] = k;
                        }
                    }
                }
            }
        }
    }
}

static std::vector<float> restore_image(const std::vector<ClusterCenter> &centers, const std::vector<int> &labels, int width, int height)
{
    std::vector<float> result(width * height * 3, 0.0f);

#pragma omp parallel for
    for (int idx = 0; idx < width * height; idx++)
    {
        int cluster_id = labels[idx];
        if (cluster_id >= 0 && cluster_id < static_cast<int>(centers.size()))
        {
            const ClusterCenter &center = centers[cluster_id];
            int pixel_idx = idx * 3;
            result[pixel_idx] = center.l;
            result[pixel_idx + 1] = center.a;
            result[pixel_idx + 2] = center.b;
        }
    }

    return result;
}

std::vector<float> apply_SLIC(std::span<const float> lab_pixels, int width, int height, int scale, int epoch)
{
    assert(width > 0 && height > 0);
    assert(lab_pixels.size() == width * height * 3);

    // 1. 初始化聚类中心
    auto centers = init_clusters(lab_pixels, width, height, scale);
    // 2. 初始化标签
    auto labels = init_labels(width, height, scale);
    // 2. 初始化距离
    auto distances = init_distances(width, height);
    // 4. 迭代优化
    for (int iter = 0; iter < epoch; iter++)
    {
        // 4.1 分配像素到最近的聚类中心
        update_distance(distances, labels, lab_pixels, centers, width, height, scale);
        // 4.2 更新聚类中心位置
        update_cluster_centers(centers, lab_pixels, labels, width, height);
    }
    // 5. 恢复图像
    auto super_pixel_image = restore_image(centers, labels, width, height);

    return super_pixel_image;
}
