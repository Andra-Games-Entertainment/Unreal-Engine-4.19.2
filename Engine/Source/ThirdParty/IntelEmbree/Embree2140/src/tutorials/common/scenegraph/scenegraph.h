// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "materials.h"
#include "lights.h"
#include "../../../include/embree2/rtcore.h"
#include "../math/random_sampler.h"

namespace embree
{  
  namespace SceneGraph
  {
    struct Node;
    struct MaterialNode;
    struct Transformations;

    Ref<Node> load(const FileName& fname, bool singleObject = false);
    void store(Ref<Node> root, const FileName& fname, bool embedTextures);
    void extend_animation(Ref<Node> node0, Ref<Node> node1);
    void optimize_animation(Ref<Node> node0);
    void set_motion_vector(Ref<Node> node, const Vec3fa& dP);
    void set_motion_vector(Ref<Node> node, const avector<Vec3fa>& motion_vector);
    void resize_randomly(RandomSampler& sampler, Ref<Node> node, const size_t N);
    Ref<Node> convert_triangles_to_quads(Ref<Node> node);
    Ref<Node> convert_quads_to_subdivs(Ref<Node> node);
    Ref<Node> convert_bezier_to_lines(Ref<Node> node);
    Ref<Node> convert_hair_to_curves(Ref<Node> node);
    
    Ref<Node> createTrianglePlane (const Vec3fa& p0, const Vec3fa& dx, const Vec3fa& dy, size_t width, size_t height, Ref<MaterialNode> material = nullptr);
    Ref<Node> createQuadPlane     (const Vec3fa& p0, const Vec3fa& dx, const Vec3fa& dy, size_t width, size_t height, Ref<MaterialNode> material = nullptr);
    Ref<Node> createSubdivPlane   (const Vec3fa& p0, const Vec3fa& dx, const Vec3fa& dy, size_t width, size_t height, float tessellationRate, Ref<MaterialNode> material = nullptr);
    Ref<Node> createTriangleSphere(const Vec3fa& center, const float radius, size_t numPhi, Ref<MaterialNode> material = nullptr);
    Ref<Node> createQuadSphere    (const Vec3fa& center, const float radius, size_t numPhi, Ref<MaterialNode> material = nullptr);
    Ref<Node> createSubdivSphere  (const Vec3fa& center, const float radius, size_t numPhi, float tessellationRate, Ref<MaterialNode> material = nullptr);
    Ref<Node> createSphereShapedHair(const Vec3fa& center, const float radius, Ref<MaterialNode> material = nullptr);
  
    Ref<Node> createHairyPlane    (int hash, const Vec3fa& pos, const Vec3fa& dx, const Vec3fa& dy, const float len, const float r, size_t numHairs, bool hair, Ref<MaterialNode> material = nullptr);

    Ref<Node> createGarbageTriangleMesh (int hash, size_t numTriangles, bool mblur, Ref<MaterialNode> material = nullptr);
    Ref<Node> createGarbageQuadMesh (int hash, size_t numQuads, bool mblur, Ref<MaterialNode> material = nullptr);
    Ref<Node> createGarbageHair (int hash, size_t numHairs, bool mblur, Ref<MaterialNode> material = nullptr);
    Ref<Node> createGarbageLineSegments (int hash, size_t numLineSegments, bool mblur, Ref<MaterialNode> material = nullptr);
    Ref<Node> createGarbageSubdivMesh (int hash, size_t numFaces, bool mblur, Ref<MaterialNode> material = nullptr);

    struct Node : public RefCount
    {
      Node (bool closed = false)
        : indegree(0), closed(closed) {}

       /* resets indegree and closed parameters */
      void reset()
      {
        std::set<Ref<Node>> done;
        resetNode(done);
      }

      /* sets material */
      virtual void setMaterial(Ref<MaterialNode> material) {};

      /* resets indegree and closed parameters */
      virtual void resetNode(std::set<Ref<Node>>& done);

      /* calculates the number of parent nodes pointing to this node */
      virtual void calculateInDegree();

      /* calculates for each node if its subtree is closed, indegrees have to be calculated first */
      virtual bool calculateClosed();

      /* checks if the node is closed */
      __forceinline bool isClosed() const { return closed; }

      /* calculates bounding box of node */
      virtual BBox3fa bounds() const {
        return empty;
      }

      /* calculates linear bounding box of node */
      virtual LBBox3fa lbounds() const {
        return empty;
      }

      /* calculates number of primitives */
      virtual size_t numPrimitives() const = 0;

      Ref<Node> set_motion_vector(const Vec3fa& dP) {
        SceneGraph::set_motion_vector(this,dP); return this;
      }

      Ref<Node> set_motion_vector(const avector<Vec3fa>& motion_vector) {
        SceneGraph::set_motion_vector(this,motion_vector); return this;
      }

    protected:
      size_t indegree;   // number of nodes pointing to us
      bool closed;       // determines if the subtree may represent an instance
    };

    struct Transformations
    {
      __forceinline Transformations() {}

      __forceinline Transformations(OneTy) {
        spaces.push_back(one);
      }

      __forceinline Transformations( size_t N ) 
        : spaces(N) {}
      
      __forceinline Transformations(const AffineSpace3fa& space) {
        spaces.push_back(space);
      }

      __forceinline Transformations(const AffineSpace3fa& space0, const AffineSpace3fa& space1) {
        spaces.push_back(space0);
        spaces.push_back(space1);
      }

      __forceinline Transformations(const avector<AffineSpace3fa>& spaces)
        : spaces(spaces) { assert(spaces.size()); }

      __forceinline size_t size() const {
        return spaces.size();
      }

      __forceinline       AffineSpace3fa& operator[] ( const size_t i )       { return spaces[i]; }
      __forceinline const AffineSpace3fa& operator[] ( const size_t i ) const { return spaces[i]; }

      BBox3fa bounds ( const BBox3fa& cbounds ) const 
      {
        BBox3fa r = empty;
        for (size_t i=0; i<spaces.size(); i++)
          r.extend(xfmBounds(spaces[i],cbounds));
        return r;
      }

      LBBox3fa lbounds ( const LBBox3fa& cbounds ) const 
      {
        assert(spaces.size());
        if (spaces.size() == 1) 
        {
          return LBBox3fa(xfmBounds(spaces[0],cbounds.bounds0),
                          xfmBounds(spaces[0],cbounds.bounds1));
        }
        else
        {
          avector<BBox3fa> bounds(spaces.size());
          for (size_t i=0; i<spaces.size(); i++) {
            const float f = float(i)/float(spaces.size()-1);
            bounds[i] = xfmBounds(spaces[i],cbounds.interpolate(f));
          }
          return LBBox3fa(bounds);
        }
      }

      void add (const Transformations& other) {
        for (size_t i=0; i<other.size(); i++) spaces.push_back(other[i]);
      }

      friend __forceinline Transformations operator* ( const Transformations& a, const Transformations& b ) 
      {
        if (a.size() == 1) 
        {
          Transformations c(b.size());
          for (size_t i=0; i<b.size(); i++) c[i] = a[0] * b[i];
          return c;
        } 
        else if (b.size() == 1) 
        {
          Transformations c(a.size());
          for (size_t i=0; i<a.size(); i++) c[i] = a[i] * b[0];
          return c;
        }
        else if (a.size() == b.size())
        {
          Transformations c(a.size());
          for (size_t i=0; i<a.size(); i++) c[i] = a[i] * b[i];
          return c;
        }
        else
          THROW_RUNTIME_ERROR("number of transformations does not match");        
      }

      AffineSpace3fa interpolate (const float gtime) const
      {
        if (spaces.size() == 1) return spaces[0];

        /* calculate time segment itime and fractional time ftime */
        const int time_segments = int(spaces.size()-1);
        const float time = gtime*float(time_segments);
        const int itime = clamp(int(floor(time)),0,time_segments-1);
        const float ftime = time - float(itime);
        return lerp(spaces[itime+0],spaces[itime+1],ftime);
      }

    public:
      avector<AffineSpace3fa> spaces;
    };
    
    struct TransformNode : public Node
    {
      ALIGNED_STRUCT;

      TransformNode (const AffineSpace3fa& xfm, const Ref<Node>& child)
        : spaces(xfm), child(child) {}

      TransformNode (const AffineSpace3fa& xfm0, const AffineSpace3fa& xfm1, const Ref<Node>& child)
        : spaces(xfm0,xfm1), child(child) {}

      TransformNode (const avector<AffineSpace3fa>& spaces, const Ref<Node>& child)
        : spaces(spaces), child(child) {}

      virtual void setMaterial(Ref<MaterialNode> material) {
        child->setMaterial(material);
      }

      virtual void resetNode(std::set<Ref<Node>>& done);
      virtual void calculateInDegree();
      virtual bool calculateClosed();
      
      virtual BBox3fa bounds() const {
        return spaces.bounds(child->bounds());
      }

      virtual LBBox3fa lbounds() const {
        return spaces.lbounds(child->lbounds());
      }

      virtual size_t numPrimitives() const {
        return child->numPrimitives();
      }

    public:
      Transformations spaces;
      Ref<Node> child;
    };
    
    struct GroupNode : public Node
    { 
      GroupNode (const size_t N = 0) { 
        children.resize(N); 
      }

      size_t size() const {
        return children.size();
      }
      
      void add(const Ref<Node>& node) {
        if (node) children.push_back(node);
      }
      
      void set(const size_t i, const Ref<Node>& node) {
        children[i] = node;
      }

      virtual BBox3fa bounds() const
      {
        BBox3fa b = empty;
        for (auto c : children) b.extend(c->bounds());
        return b;
      }

      virtual LBBox3fa lbounds() const
      {
        LBBox3fa b = empty;
        for (auto c : children) b.extend(c->lbounds());
        return b;
      }

      virtual size_t numPrimitives() const 
      {
        size_t n = 0;
        for (auto child : children) n += child->numPrimitives();
        return n;
      }

      void triangles_to_quads()
      {
        for (size_t i=0; i<children.size(); i++)
          children[i] = convert_triangles_to_quads(children[i]);
      }

      void quads_to_subdivs()
      {
        for (size_t i=0; i<children.size(); i++)
          children[i] = convert_quads_to_subdivs(children[i]);
      }
      
      void bezier_to_lines()
      {
        for (size_t i=0; i<children.size(); i++)
          children[i] = convert_bezier_to_lines(children[i]);
      }

      void hair_to_curves()
      {
        for (size_t i=0; i<children.size(); i++)
          children[i] = convert_hair_to_curves(children[i]);
      }

      virtual void setMaterial(Ref<MaterialNode> material) {
        for (auto& child : children) child->setMaterial(material);
      }

      virtual void resetNode(std::set<Ref<Node>>& done);
      virtual void calculateInDegree();
      virtual bool calculateClosed();
      
    public:
      std::vector<Ref<Node> > children;
    };
    
    struct LightNode : public Node
    {
      LightNode (Ref<Light> light)
        : light(light) {}

      virtual size_t numPrimitives() const {
        return 0;
      }

      Ref<Light> light;
    };
    
    struct MaterialNode : public Node
    {
      ALIGNED_STRUCT;

      MaterialNode(const Material& material)
        : material(material) {}

      virtual size_t numPrimitives() const {
        return 0;
      }

      Material material;
    };
    
    /*! Mesh. */
    struct TriangleMeshNode : public Node
    {
      typedef Vec3fa Vertex;

      struct Triangle 
      {
      public:
        Triangle() {}
        Triangle (unsigned v0, unsigned v1, unsigned v2) 
        : v0(v0), v1(v1), v2(v2) {}
      public:
        unsigned v0, v1, v2;
      };
      
    public:
      TriangleMeshNode (Ref<MaterialNode> material, size_t numTimeSteps = 0) 
        : Node(true), material(material) 
      {
        for (size_t i=0; i<numTimeSteps; i++)
          positions.push_back(avector<Vertex>());
      }

      TriangleMeshNode (Ref<SceneGraph::TriangleMeshNode> imesh, const Transformations& spaces)
        : Node(true), normals(imesh->normals), texcoords(imesh->texcoords), triangles(imesh->triangles), material(imesh->material)
      {
        for (size_t i=0; i<spaces.size(); i++) {
          avector<Vertex> verts(imesh->numVertices());
          for (size_t j=0; j<imesh->numVertices(); j++) 
            verts[j] = xfmPoint(spaces[i],imesh->positions[0][j]);
          positions.push_back(std::move(verts));
        }
      }
      
      virtual void setMaterial(Ref<MaterialNode> material) {
        this->material = material;
      }

      virtual BBox3fa bounds() const
      {
        BBox3fa b = empty;
        for (const auto& p : positions)
          for (auto x : p)
            b.extend(x);
        return b;
      }

      virtual LBBox3fa lbounds() const
      {
        avector<BBox3fa> bboxes(positions.size());
        for (size_t t=0; t<positions.size(); t++) {
          BBox3fa b = empty;
          for (auto x : positions[t]) b.extend(x);
          bboxes[t] = b;
        }
        return LBBox3fa(bboxes);
      }

      virtual size_t numPrimitives() const {
        return triangles.size();
      }

      size_t numVertices() const {
        assert(positions.size());
        return positions[0].size();
      }

      size_t numTimeSteps() const {
        return positions.size();
      }

      void verify() const;

    public:
      std::vector<avector<Vertex>> positions;
      avector<Vertex> normals;
      std::vector<Vec2f> texcoords;
      std::vector<Triangle> triangles;
      Ref<MaterialNode> material;
    };

    /*! Mesh. */
    struct QuadMeshNode : public Node
    {
      typedef Vec3fa Vertex;

      struct Quad
      {
      public:
        Quad() {}
        Quad (unsigned v0, unsigned v1, unsigned v2, unsigned v3) 
        : v0(v0), v1(v1), v2(v2), v3(v3) {}
      public:
        unsigned v0, v1, v2, v3;
      };
      
    public:
      QuadMeshNode (Ref<MaterialNode> material, size_t numTimeSteps = 0) 
        : Node(true), material(material) 
      {
        for (size_t i=0; i<numTimeSteps; i++)
          positions.push_back(avector<Vertex>());
      }

      QuadMeshNode (Ref<SceneGraph::QuadMeshNode> imesh, const Transformations& spaces)
        : Node(true), normals(imesh->normals), texcoords(imesh->texcoords), quads(imesh->quads), material(imesh->material)
      {
        for (size_t i=0; i<spaces.size(); i++) {
          avector<Vertex> verts(imesh->numVertices());
          for (size_t j=0; j<imesh->numVertices(); j++) 
            verts[j] = xfmPoint(spaces[i],imesh->positions[0][j]);
          positions.push_back(std::move(verts));
        }
      }
      
      virtual void setMaterial(Ref<MaterialNode> material) {
        this->material = material;
      }

      virtual BBox3fa bounds() const
      {
        BBox3fa b = empty;
        for (const auto& p : positions)
          for (auto x : p)
            b.extend(x);
        return b;
      }

      virtual LBBox3fa lbounds() const
      {
        avector<BBox3fa> bboxes(positions.size());
        for (size_t t=0; t<positions.size(); t++) {
          BBox3fa b = empty;
          for (auto x : positions[t]) b.extend(x);
          bboxes[t] = b;
        }
        return LBBox3fa(bboxes);
      }
      
      virtual size_t numPrimitives() const {
        return quads.size();
      }

      size_t numVertices() const {
        assert(positions.size());
        return positions[0].size();
      }

      size_t numTimeSteps() const {
        return positions.size();
      }

      void verify() const;

    public:
      std::vector<avector<Vertex>> positions;
      avector<Vertex> normals;
      std::vector<Vec2f> texcoords;
      std::vector<Quad> quads;
      Ref<MaterialNode> material;
    };

    /*! Subdivision Mesh. */
    struct SubdivMeshNode : public Node
    {
      typedef Vec3fa Vertex;

      SubdivMeshNode (Ref<MaterialNode> material, size_t numTimeSteps = 0) 
        : Node(true), 
          position_subdiv_mode(RTC_SUBDIV_SMOOTH_BOUNDARY), 
          normal_subdiv_mode(RTC_SUBDIV_SMOOTH_BOUNDARY),
          texcoord_subdiv_mode(RTC_SUBDIV_SMOOTH_BOUNDARY),
          material(material), tessellationRate(2.0f) 
      {
        for (size_t i=0; i<numTimeSteps; i++)
          positions.push_back(avector<Vertex>());
      }

      SubdivMeshNode (Ref<SceneGraph::SubdivMeshNode> imesh, const Transformations& spaces)
        : Node(true), 
        normals(imesh->normals),
        texcoords(imesh->texcoords),
        position_indices(imesh->position_indices),
        normal_indices(imesh->normal_indices),
        texcoord_indices(imesh->texcoord_indices),
        position_subdiv_mode(imesh->position_subdiv_mode), 
        normal_subdiv_mode(imesh->normal_subdiv_mode),
        texcoord_subdiv_mode(imesh->texcoord_subdiv_mode),
        verticesPerFace(imesh->verticesPerFace),
        holes(imesh->holes),
        edge_creases(imesh->edge_creases),
        edge_crease_weights(imesh->edge_crease_weights),
        vertex_creases(imesh->vertex_creases),
        vertex_crease_weights(imesh->vertex_crease_weights),
        material(imesh->material), 
        tessellationRate(imesh->tessellationRate)
      {
        for (size_t i=0; i<spaces.size(); i++) {
          avector<Vertex> verts(imesh->numPositions());
          for (size_t j=0; j<imesh->numPositions(); j++) 
            verts[j] = xfmPoint(spaces[i],imesh->positions[0][j]);
          positions.push_back(std::move(verts));
        }
      }
      
      virtual void setMaterial(Ref<MaterialNode> material) {
        this->material = material;
      }

      virtual BBox3fa bounds() const
      {
        BBox3fa b = empty;
        for (const auto& p : positions)
          for (auto x : p)
            b.extend(x);
        return b;
      }

      virtual LBBox3fa lbounds() const
      {
        avector<BBox3fa> bboxes(positions.size());
        for (size_t t=0; t<positions.size(); t++) {
          BBox3fa b = empty;
          for (auto x : positions[t]) b.extend(x);
          bboxes[t] = b;
        }
        return LBBox3fa(bboxes);
      }

      virtual size_t numPrimitives() const {
        return verticesPerFace.size();
      }

      size_t numPositions() const {
        assert(positions.size());
        return positions[0].size();
      }

      size_t numTimeSteps() const {
        return positions.size();
      }

      void verify() const;

    public:
      std::vector<avector<Vertex>> positions; //!< vertex positions for multiple timesteps
      avector<Vertex> normals;              //!< face vertex normals
      std::vector<Vec2f> texcoords;             //!< face texture coordinates
      std::vector<unsigned> position_indices;        //!< position indices for all faces
      std::vector<unsigned> normal_indices;          //!< normal indices for all faces
      std::vector<unsigned> texcoord_indices;        //!< texcoord indices for all faces
      RTCSubdivisionMode position_subdiv_mode;  
      RTCSubdivisionMode normal_subdiv_mode;
      RTCSubdivisionMode texcoord_subdiv_mode;
      std::vector<unsigned> verticesPerFace;         //!< number of indices of each face
      std::vector<unsigned> holes;                   //!< face ID of holes
      std::vector<Vec2i> edge_creases;          //!< index pairs for edge crease 
      std::vector<float> edge_crease_weights;   //!< weight for each edge crease
      std::vector<unsigned> vertex_creases;          //!< indices of vertex creases
      std::vector<float> vertex_crease_weights; //!< weight for each vertex crease
      Ref<MaterialNode> material;
      float tessellationRate;
    };

    /*! Line Segments */
    struct LineSegmentsNode : public Node
    {
      typedef Vec3fa Vertex;

    public:
      LineSegmentsNode (Ref<MaterialNode> material, size_t numTimeSteps = 0)
        : Node(true), material(material) 
      {
        for (size_t i=0; i<numTimeSteps; i++)
          positions.push_back(avector<Vertex>());
      }

      LineSegmentsNode (Ref<SceneGraph::LineSegmentsNode> imesh, const Transformations& spaces)
        : Node(true), indices(imesh->indices), material(imesh->material)
      {
        for (size_t i=0; i<spaces.size(); i++) {
          avector<Vertex> verts(imesh->numVertices());
          for (size_t j=0; j<imesh->numVertices(); j++) 
            verts[j] = xfmPoint(spaces[i],imesh->positions[0][j]);
          positions.push_back(std::move(verts));
        }
      }
      
      virtual void setMaterial(Ref<MaterialNode> material) {
        this->material = material;
      }

      virtual BBox3fa bounds() const
      {
        BBox3fa b = empty;
        for (const auto& p : positions)
          for (auto x : p)
            b.extend(x);
        return b;
      }

      virtual LBBox3fa lbounds() const
      {
        avector<BBox3fa> bboxes(positions.size());
        for (size_t t=0; t<positions.size(); t++) {
          BBox3fa b = empty;
          for (auto x : positions[t]) b.extend(x);
          bboxes[t] = b;
        }
        return LBBox3fa(bboxes);
      }

      virtual size_t numPrimitives() const {
        return indices.size();
      }

      size_t numVertices() const {
        assert(positions.size());
        return positions[0].size();
      }

      size_t numTimeSteps() const {
        return positions.size();
      }

      void verify() const;

    public:
      std::vector<avector<Vertex>> positions; //!< line control points (x,y,z,r) for multiple timesteps
      std::vector<unsigned> indices; //!< list of line segments
      Ref<MaterialNode> material;
    };

    /*! Hair Set. */
    struct HairSetNode : public Node
    {
      typedef Vec3fa Vertex;

      struct Hair
      {
      public:
        Hair () {}
        Hair (unsigned vertex, unsigned id) 
          : vertex(vertex), id(id) {}

      public:
        unsigned vertex,id;  //!< index of first control point and hair ID
      };
      
    public:
      HairSetNode (bool hair, Ref<MaterialNode> material, size_t numTimeSteps = 0)
        : Node(true), hair(hair), material(material), tessellation_rate(4)
      {
        for (size_t i=0; i<numTimeSteps; i++)
          positions.push_back(avector<Vertex>());
      }

      HairSetNode (Ref<SceneGraph::HairSetNode> imesh, const Transformations& spaces)
        : Node(true), hair(imesh->hair), hairs(imesh->hairs), material(imesh->material), tessellation_rate(imesh->tessellation_rate)
      {
        for (size_t i=0; i<spaces.size(); i++) {
          avector<Vertex> verts(imesh->numVertices());
          for (size_t j=0; j<imesh->numVertices(); j++) 
            verts[j] = xfmPoint(spaces[i],imesh->positions[0][j]);
          positions.push_back(std::move(verts));
        }
      }

      virtual void setMaterial(Ref<MaterialNode> material) {
        this->material = material;
      }

      virtual BBox3fa bounds() const
      {
        BBox3fa b = empty;
        for (const auto& p : positions)
          for (auto x : p)
            b.extend(x);
        return b;
      }

      virtual LBBox3fa lbounds() const
      {
        avector<BBox3fa> bboxes(positions.size());
        for (size_t t=0; t<positions.size(); t++) {
          BBox3fa b = empty;
          for (auto x : positions[t]) b.extend(x);
          bboxes[t] = b;
        }
        return LBBox3fa(bboxes);
      }

      virtual size_t numPrimitives() const {
        return hairs.size();
      }

      size_t numVertices() const {
        assert(positions.size());
        return positions[0].size();
      }

      size_t numTimeSteps() const {
        return positions.size();
      }

      void verify() const;

    public:
      bool hair;                //!< true is this is hair geometry, false if this are curves
      std::vector<avector<Vertex>> positions; //!< hair control points (x,y,z,r) for multiple timesteps
      std::vector<Hair> hairs;  //!< list of hairs
      Ref<MaterialNode> material;
      unsigned tessellation_rate;
    };
    
    Ref<Node> flatten(Ref<Node> node, const Transformations& spaces = Transformations(one));
  }
}
