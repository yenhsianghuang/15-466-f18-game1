#include "WalkMesh.hpp"
#include "read_chunk.hpp"

#include <glm/glm.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

glm::vec3 barycentric(glm::vec3 const &a, glm::vec3 const &b,
                      glm::vec3 const &c, glm::vec3 const &p) {
    glm::vec3 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);
    float invDenom = 1.0 / (d00 * d11 - d01 * d01);
    float v = (d11 * d20 - d01 * d21) * invDenom;
    float w = (d00 * d21 - d01 * d20) * invDenom;
    float u = 1.0f - v - w;
    return glm::vec3(u, v, w);
}

bool inTriangle(glm::vec3 const &weights) {
    return !(weights[0] < 0.0f || weights[1] < 0.0f || weights[2] < 0.0f);
}

void printVertex(glm::vec3 const &vtx) {
    std::cout << vtx[0] << " " << vtx[1] << " " << vtx[2] << std::endl;
}

void printTriangle(std::vector< glm::vec3 > tri) {
    for (auto& t : tri) {
        printVertex(t);
    }
    std::cout << std::endl;
}

// from MeshBuffer
WalkMesh::WalkMesh(std::string filename) {
    std::ifstream file(filename, std::ios::binary);
    read_chunk(file, "vtx0", &vertices);
    read_chunk(file, "nom0", &vertex_normals);
    read_chunk(file, "lpi0", &triangles);

    // insert (a,b)->c, (b,c)->a, (c,a)->b
    for (auto &tri : triangles) {
        auto a = tri[0], b = tri[1], c = tri[2];
        next_vertex[glm::uvec2(a, b)] = c;
        next_vertex[glm::uvec2(b, c)] = a;
        next_vertex[glm::uvec2(c, a)] = b;
    }
}

// WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector<
// glm::uvec3 > const &triangles_) : vertices(vertices_), triangles(triangles_) {
////TODO: construct next_vertex map
//}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
    WalkPoint closest;

    // optimized algorithm based on my game
    // assumption: I can find at least one triangle where the projection of
    // world_point lies
    // TODO: iterate through triangles
    // TODO: find the projection P of world_point onto the triangle plane
    // TODO: find the barycentric coordinate
    // TODO: if P is in the triangle and the distance between world_point and P
    // is shorter, update closest
    // TODO: report error if world_point cannot be projected onto any of the
    // triangles
    float min_dist = std::numeric_limits<float>::max();

    for (auto &tri : triangles) {
        glm::vec3 vertex1 = vertices[tri[0]];
        glm::vec3 vertex2 = vertices[tri[1]];
        glm::vec3 vertex3 = vertices[tri[2]];
        glm::vec3 normal =
            glm::normalize(glm::cross(vertex2 - vertex1, vertex3 - vertex1));
        auto t =
            glm::dot(vertex1 - world_point, normal) / glm::dot(normal, normal);
        glm::vec3 projection = world_point + t * normal;
        glm::vec3 weights = barycentric(vertex1, vertex2, vertex3, projection);

        // check whether this weight is in the triangle
        if (!inTriangle(weights)) {
            continue;
        }

        // if the distance is shorter whan min_dist, update closest
        auto dist = glm::distance(world_point, projection);
        if (dist < min_dist) {
            closest.triangle = tri;
            closest.weights = weights;
            min_dist = dist;
        }
    }

    assert(min_dist < std::numeric_limits<float>::max());
    return closest;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
    // TODO: project step to barycentric coordinates to get weights_step
    // glm::vec3 weights_step;
    glm::vec3 target_weights =
        barycentric(vertices[wp.triangle[0]], vertices[wp.triangle[1]],
                    vertices[wp.triangle[2]], world_point(wp) + step);
    //glm::vec3 weights_step = target_weights - wp.weights;


    if (inTriangle(target_weights)) {  // if a triangle edge is not crossed
        wp.weights = target_weights;
    } else {  // if a triangle edge is crossed
              // TODO: wp.weights gets moved to triangle edge, and step gets
              // reduced if there is another triangle over the edge:
              // TODO: wp.triangle gets updated to adjacent triangle
              // TODO: step gets rotated over the edge
              // else if there is no other triangle over the edge:
              // TODO: wp.triangle stays the same.
              // TODO: step gets updated to slide along the edge
    }
}
