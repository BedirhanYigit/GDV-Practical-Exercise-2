// ========================================================================= //
// Authors: Daniel Rutz, Daniel Ströter, Roman Getto, Matthias Bein          //
//                                                                           //
// GRIS - Graphisch Interaktive Systeme                                      //
// Technische Universität Darmstadt                                          //
// Fraunhoferstrasse 5                                                       //
// D-64283 Darmstadt, Germany                                                //
//                                                                           //
// Content: Simple class for reading and rendering triangle meshes           //
// ========================================================================= //

#include <cfloat>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <QtMath>
#include <QOpenGLContext>
#include <QOpenGLFunctions_2_1>

#include "trianglemesh.h"

// =========================
// === PRIVATE FUNCTIONS ===
// =========================

void TriangleMesh::calculateNormals() {
    normals.clear();
    normals.resize(vertices.size());
    for (const auto& face : triangles) {
        const GLuint iX = face[0];
        const GLuint iY = face[1];
        const GLuint iZ = face[2];

        const Vertex& vecX = vertices[iX];
        const Vertex& vecY = vertices[iY];
        const Vertex& vecZ = vertices[iZ];

        const Vertex vec1 = vecY - vecX;
        const Vertex vec2 = vecZ - vecX;
        const Vertex normal = cross(vec1, vec2);

        normals[iX] += normal;
        normals[iY] += normal;
        normals[iZ] += normal;
    }

    for (auto& normal : normals) {
        normal.normalize();
    }
}

void TriangleMesh::rewriteAllVBOs() {
    //TODO: Fill VBO buffers with data (Ex. 2)
    if (VBOv == 0 || VBOn == 0 || VBOf == 0) return; // Safety check

    // --- 1. Vertex Buffer (VBOv) ---
    // Bind the buffer to the GL_ARRAY_BUFFER target
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOv);
    // Copy the vertex data to the buffer bound to GL_ARRAY_BUFFER
    f->glBufferData(
        GL_ARRAY_BUFFER,                       // Target (Vertex Data Buffer)
        vertices.size() * sizeof(Vertex),      // Size of the data in bytes
        (const GLvoid*)vertices.data(),        // Pointer to the data (explicitly cast to GLvoid*)
        GL_STATIC_DRAW                         
    );

    // --- 2. Normal Buffer (VBOn) ---
    // Bind the buffer to the GL_ARRAY_BUFFER target
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOn);
    // Copy the normal data to the buffer
    f->glBufferData(
        GL_ARRAY_BUFFER,
        normals.size() * sizeof(Vertex),
        (const GLvoid*)normals.data(),        
        GL_STATIC_DRAW
    );

    // --- 3. Face/Index Buffer (VBOf) ---
    // Calculate the size of the index data in bytes: 
    // (number of faces) * (3 indices per face) * (size of one index: unsigned int)
    size_t index_data_size = triangles.size() * 3 * sizeof(unsigned int);

    // Bind the buffer to the GL_ELEMENT_ARRAY_BUFFER target (for indices)
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBOf);
    // Copy the index data to the buffer
    f->glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        index_data_size,
        (const GLvoid*)triangles.data(),       // Pointer to the data (explicitly cast to GLvoid*)
        GL_STATIC_DRAW
    );

    // Unbind the buffers to prevent accidental modification
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

}

void TriangleMesh::createAllVBOs() {
	//TODO: Generate Buffers (Ex. 2)
    f->glGenBuffers(1, &VBOv); // Generate ID for Vertex Buffer
    f->glGenBuffers(1, &VBOn); // Generate ID for Normal Buffer
    f->glGenBuffers(1, &VBOf); // Generate ID for Index Buffer
    rewriteAllVBOs();
}

void TriangleMesh::cleanupVBO() {
	// delete VBO
    if (VBOv != 0) f->glDeleteBuffers(1, &VBOv);
	if (VBOn != 0) f->glDeleteBuffers(1, &VBOn);
	if (VBOf != 0) f->glDeleteBuffers(1, &VBOf);
	VBOv = 0;
	VBOn = 0;
	VBOf = 0;
}

void TriangleMesh::cleanup() {
	// clear mesh data
	vertices.clear();
	triangles.clear();
	normals.clear();
	// clear bounding box data
	boundingBoxMin = Vec3f(FLT_MAX, FLT_MAX, FLT_MAX);
	boundingBoxMax = Vec3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// clear VBO data
	cleanupVBO();
}

// ===============================
// === CONSTRUCTOR, DESTRUCTOR ===
// ===============================

TriangleMesh::TriangleMesh(QOpenGLFunctions_2_1* f) :
	VBOv(0), VBOn(0), VBOf(0), f(f)
{ }

TriangleMesh::~TriangleMesh()
{
	// delete VBO
	if (VBOv != 0) f->glDeleteBuffers(1, &VBOv);
	if (VBOn != 0) f->glDeleteBuffers(1, &VBOn);
	if (VBOf != 0) f->glDeleteBuffers(1, &VBOf);
}

// ================
// === RAW DATA ===
// ================

void TriangleMesh::flipNormals() {
  for (auto& normal : normals) {
    normal *= -1.0;
  }
}

void TriangleMesh::translateToCenter(const Vec3f& newBBmid) {
	Vec3f trans = newBBmid - boundingBoxMid;
	for (auto& vertex : vertices) {
        vertex += trans;
    }
	boundingBoxMin += trans;
	boundingBoxMax += trans;
	boundingBoxMid += trans;
	// data changed => fix VBOs
	rewriteAllVBOs();
}

void TriangleMesh::scaleToLength(const float newLength) {
	float length = std::max(std::max(boundingBoxSize.x(), boundingBoxSize.y()), boundingBoxSize.z());
	float scale = newLength / length;
	for (auto& vertex : vertices) {
        vertex *= scale;
    }
	boundingBoxMin *= scale;
	boundingBoxMax *= scale;
	boundingBoxMid *= scale;
	boundingBoxSize *= scale;
	// data changed => fix VBOs
	rewriteAllVBOs();
}

// =================
// === LOAD MESH ===
// =================

void TriangleMesh::loadOBJ(const char* filename) 
{
    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cout << "loadOBJ: can not find " << filename << std::endl;
        return;
    }

    vertices.resize(0);
    triangles.resize(0);
    normals.resize(0);

    // Load all vertices and triangles and ignore other entries.
    while (!in.eof()) 
    {
        std::string s;        
        in >> s;

        if (s == "v") {
            // read and store a vertex
            GLfloat x, y, z;
            in >> x >> y >> z;
            vertices.push_back({ x, y, z });

			// update bounding box
			boundingBoxMin[0] = std::min(x, boundingBoxMin[0]);
			boundingBoxMin[1] = std::min(y, boundingBoxMin[1]);
			boundingBoxMin[2] = std::min(z, boundingBoxMin[2]);
			boundingBoxMax[0] = std::max(x, boundingBoxMax[0]);
			boundingBoxMax[1] = std::max(y, boundingBoxMax[1]);
			boundingBoxMax[2] = std::max(z, boundingBoxMax[2]);
			
			// Ignore rest of line
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
        } else if (s == "vn") {
            // read and store a vertex normal
            GLfloat x, y, z;
            in >> x >> y >> z;
            normals.push_back({ x, y, z });

			// Ignore rest of line
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
        } else if (s == "f") 
        {
            // read and store a face
            std::string line;
            getline(in, line);
            std::stringstream ss(line);
            std::vector<int> indices;
            int num;

            while (ss >> num) {
                // convert negative indices to positive indices
                if (num < 0) num = vertices.size() + 1 + num;

                // Store and convert OBJ's 1-based indices to the required 0-based indices!
                indices.push_back(num - 1);
            }
            if (indices.size() == 3)
                triangles.push_back({ (unsigned int) indices[0], (unsigned int)indices[1], (unsigned int)indices[2]});
            else
                qWarning("The OBJ file contains polygons that are not triangles! Ignoring "
                        "entry, this will lead to holes in your mesh!");
        } else 
        {
            // Skip comments and other entries
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

    }

	// update bounding box
	boundingBoxMid = 0.5f*boundingBoxMin + 0.5f*boundingBoxMax;
	boundingBoxSize = boundingBoxMax - boundingBoxMin;

	// close ifstream
	in.close();

    // calculate normals if they are not present in the file
    if(normals.size() != vertices.size())
        calculateNormals();
		
	// createVBO
	createAllVBOs();
}

void TriangleMesh::loadOBJ(const char* filename, const Vec3f& BBmid, const float BBlength) {
	loadOBJ(filename);
	translateToCenter(BBmid);
	scaleToLength(BBlength);
}

// ==============
// === RENDER ===
// ==============

void TriangleMesh::drawImmediate() {
    if (triangles.empty()) return;

    f->glBegin(GL_TRIANGLES);
    for (const auto& face : triangles) {
        const auto& vertex1 = vertices[face[0]];
        const auto& vertex2 = vertices[face[1]];
        const auto& vertex3 = vertices[face[2]];

        const auto& normal1 = normals[face[0]];
        const auto& normal2 = normals[face[1]];
        const auto& normal3 = normals[face[2]];

        f->glNormal3f(normal1.x(), normal1.y(), normal1.z());
        f->glVertex3f(vertex1.x(), vertex1.y(), vertex1.z());

        f->glNormal3f(normal2.x(), normal2.y(), normal2.z());
        f->glVertex3f(vertex2.x(), vertex2.y(), vertex2.z());

        f->glNormal3f(normal3.x(), normal3.y(), normal3.z());
        f->glVertex3f(vertex3.x(), vertex3.y(), vertex3.z());
    }
    f->glEnd();
}

void TriangleMesh::drawArray() {
	if (triangles.empty()) return;
	//TODO: Implement Array Mode (Ex. 1)
    // 1. Enable Client States for Vertex and Normal Arrays
    f->glEnableClientState(GL_VERTEX_ARRAY);
    f->glEnableClientState(GL_NORMAL_ARRAY);

    // 2. Pass Pointers to the Vertex and Normal Data
    // We specify the size (3 components), type (GL_FLOAT), stride (0 for tightly packed),
    // and the pointer to the start of the data buffer.
    f->glVertexPointer(3, GL_FLOAT, 0, vertices.data());
    f->glNormalPointer(GL_FLOAT, 0, normals.data());

    // 3. Draw the Model using Indexed Vertices (GL_TRIANGLES)
    // We use glDrawElements to draw the triangles by looking up the indices 
    // in the 'triangles' array.
    f->glDrawElements(
        GL_TRIANGLES, 
        triangles.size() * 3, // Total number of indices (faces * 3 indices/face)
        GL_UNSIGNED_INT,      // The data type of the indices
        triangles.data()      // Pointer to the index data
    );

    // 4. Disable Client States
    f->glDisableClientState(GL_NORMAL_ARRAY);
    f->glDisableClientState(GL_VERTEX_ARRAY);
}

void TriangleMesh::drawVBO() {
	if (triangles.empty()) return;
	if (VBOv == 0 || VBOn == 0 || VBOf == 0) return;
    //TODO: Implement VBO Mode (Ex. 2)

    if (triangles.empty()) return;
    if (VBOv == 0 || VBOn == 0 || VBOf == 0) return;

    // 1. Enable Client States
    f->glEnableClientState(GL_VERTEX_ARRAY);
    f->glEnableClientState(GL_NORMAL_ARRAY);

    // --- Vertex Buffer Setup ---
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOv);
    // VBO: The last argument must be an offset, explicitly cast to (const GLvoid*)0
    f->glVertexPointer(3, GL_FLOAT, 0, (const GLvoid*)0); 

    // --- Normal Buffer Setup ---
    f->glBindBuffer(GL_ARRAY_BUFFER, VBOn);
    // VBO: The last argument must be an offset, explicitly cast to (const GLvoid*)0
    f->glNormalPointer(GL_FLOAT, 0, (const GLvoid*)0); 
    
    // --- Index Buffer Setup ---
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBOf);

    // 5. Draw the Elements
    f->glDrawElements(
        GL_TRIANGLES,
        triangles.size() * 3, 
        GL_UNSIGNED_INT,      
        (const GLvoid*)0      // VBO: Index offset, explicitly cast to (const GLvoid*)0
    );

    // 6. Cleanup: Unbind buffers and disable states
    f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    f->glDisableClientState(GL_NORMAL_ARRAY);
    f->glDisableClientState(GL_VERTEX_ARRAY);
}
