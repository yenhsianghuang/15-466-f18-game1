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
    return (weights[0] < 0.0f || weights[1] < 0.0f || weights[2] < 0.0f);
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

    std::cout << "vertices size: " << vertices.size() << std::endl;
    std::cout << "vertex_normals size: " << vertex_normals.size() << std::endl;
    std::cout << "triangles size: " << triangles.size() << std::endl;
}

// WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector<
// glm::uvec3 > const &triangles_) : vertices(vertices_), triangles(triangles_) {
////TODO: construct next_vertex map
//}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
    WalkPoint closest;
    // TODO: iterate through triangles
    // TODO: for each triangle, find closest point on triangle to world_point
    // TODO: if point is closest, closest.triangle gets the current triangle,
    // closest.weights gets the barycentric coordinates

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
        printTriangle(std::vector< glm::vec3 > {vertex1, vertex2, vertex3});
        glm::vec3 normal =
            glm::normalize(glm::cross(vertex2 - vertex1, vertex3 - vertex1));
        auto t =
            glm::dot(vertex1 - world_point, normal) / glm::dot(normal, normal);
        glm::vec3 projection = world_point + t * normal;
        //std::cout << "projection " << projection[0] << " " << projection[1] << " " << projection[2] << std::endl;
        glm::vec3 weights = barycentric(vertex1, vertex2, vertex3, projection);
        //std::cout << "weights " << weights[0] << " " << weights[1] << " " << weights[2] << std::endl;

        // check whether this weight is in the triangle
        if (!inTriangle(weights)) {
            continue;
        }
        //if (weights[0] < 0.0f || weights[1] < 0.0f || weights[2] < 0.0f) {
            //continue;
        //}

        // if the distance is shorter whan min_dist, update closest
        auto dist = glm::distance(world_point, projection);
        if (dist < min_dist) {
            closest.triangle = tri;
            closest.weights = weights;
            min_dist = dist;
        }
    }

    {  //test barycentric, it is correct...
        glm::vec3 test_world_point = glm::vec3(9.335f, 8.968f, 2.027f);
        glm::vec3 vertex1 = glm::vec3(9.46f, 8.4f, 1.07f);
        glm::vec3 vertex2 = glm::vec3(9.46f, 9.53f, 1.07f);
        glm::vec3 vertex3 = glm::vec3(6.66f, 8.4f, 1.07f);
        glm::vec3 normal =
            glm::normalize(glm::cross(vertex2 - vertex1, vertex3 - vertex1));
        auto t =
            glm::dot(vertex1 - test_world_point, normal) / glm::dot(normal, normal);
        glm::vec3 projection = test_world_point + t * normal;

        glm::vec3 test =
            barycentric(vertex1, vertex2, vertex3, projection);

        std::cout << "test projection " << projection[0] << " " << projection[1] << " " << projection[2] << std::endl;
        std::cout << "test weight " << test[0] << " " << test[1] << " " << test[2] << std::endl;
    }


    assert(min_dist < std::numeric_limits<float>::max());

    {  //display final triangle
    std::cout << "final weights " << closest.weights[0] << " " << closest.weights[1] << " " << closest.weights[2] << std::endl;

    glm::vec3 vertex1 = vertices[closest.triangle[0]];
    glm::vec3 vertex2 = vertices[closest.triangle[1]];
    glm::vec3 vertex3 = vertices[closest.triangle[2]];

    std::cout << "final triangle " << std::endl;
    std::cout << vertex1[0] << " " << vertex1[1] << " " << vertex1[2] << std::endl;
    std::cout << vertex2[0] << " " << vertex2[1] << " " << vertex2[2] << std::endl;
    std::cout << vertex3[0] << " " << vertex3[1] << " " << vertex3[2] << std::endl;

    }

    return closest;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
    // TODO: project step to barycentric coordinates to get weights_step
    // glm::vec3 weights_step;
    //glm::vec3 target_weights =
        //barycentric(vertices[wp.triangle[0]], vertices[wp.triangle[1]],
                    //vertices[wp.triangle[2]], world_point(wp) + step);
    //glm::vec3 weights_step = target_weights - wp.weights;

    // TODO: when does wp.weights + t * weights_step cross a triangle edge?
    float t = 1.0f;

    if (t >= 1.0f) {  // if a triangle edge is not crossed
        // TODO: wp.weights gets moved by weights_step, nothing else needs to be
        // done.

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
