#!/usr/bin/env python

#based on 'export-sprites.py' and 'glsprite.py' from TCHOW Rainbow; code used is released into the public domain.

#Note: Script meant to be executed from within blender, as per:
#blender --background --python export-meshes.py -- <infile.blend>[:layer] <outfile.p[n][c][t]>

import sys,re

args = []
for i in range(0,len(sys.argv)):
        if sys.argv[i] == '--':
                args = sys.argv[i+1:]

if len(args) != 2:
        print("\n\nUsage:\nblender --background --python export-walkmesh.py -- <infile.blend>[:layer] <outfile.blob>\nExports the walkmesh in layer (default 2) to a binary blob.\n")
        exit(1)

infile = args[0]
layer = 2
m = re.match(r'^(.*):(\d+)$', infile)
if m:
        infile = m.group(1)
        layer = int(m.group(2))
outfile = args[1]

assert layer >= 1 and layer <= 20

print("Will export walkmesh from layer " + str(layer) + " of '" + infile + "' to '" + outfile + "'.")

import bpy
import struct

import argparse


bpy.ops.wm.open_mainfile(filepath=infile)

#Blob file format:
# vtx0
# nom0
# lpi0

#vertex contains vertex of the walkmesh:
vertex = b''

#normal contains normal of the walkmesh:
normal = b''

#loop_indices contains indices of triangle vertex:
loop_indices = b''

vertex_count = 0
for obj in bpy.data.objects:
        if obj.layers[layer-1] and obj.type == 'MESH' and obj.name == 'WalkMesh':

                mesh = obj.data
                print("Writing '" + mesh.name + "'...")

                # This part was taught by Eric
                for vtx in mesh.vertices:
                    for x in vtx.co:
                        vertex += struct.pack('f', x)
                    for n in vtx.normal:
                        normal += struct.pack('f', n)

                for poly in mesh.polygons:
                        assert(len(poly.vertices) == 3)
                        loop_indices += struct.pack('III', poly.vertices[0], poly.vertices[1], poly.vertices[2])
                vertex_count += len(mesh.vertices)  #3 vertices for each polygon

                break

        else:  #skip if obj.data is not walkmesh
                continue

#check that we wrote as much data as anticipated:
assert(vertex_count * 3 * 4 == len(vertex))
assert(vertex_count * 3 * 4 == len(normal))
assert(len(obj.data.polygons) * 3 * 4 == len(loop_indices))

#write the data chunk and index chunk to an output blob:
blob = open(outfile, 'wb')

def write_chunk(magic, data):
        blob.write(struct.pack('4s',magic)) #type
        blob.write(struct.pack('I', len(data))) #length
        blob.write(data)

write_chunk(b'vtx0', vertex)
write_chunk(b'nom0', normal)
write_chunk(b'lpi0', loop_indices)

print("Wrote " + str(blob.tell()) + " bytes to '" + outfile + "'")
blob.close()
