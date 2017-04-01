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

#include "../common/math/random_sampler.h"
#include "../common/math/sampling.h"
#include "../common/core/differential_geometry.h"
#include "../common/tutorial/tutorial_device.h"
#include "../common/tutorial/scene_device.h"
#include "../common/tutorial/optics.h"

namespace embree {

#undef TILE_SIZE_X
#undef TILE_SIZE_Y

#define TILE_SIZE_X 4
#define TILE_SIZE_Y 4

#define FIXED_SAMPLING 0
#define SAMPLES_PER_PIXEL 1

#define FIXED_EDGE_TESSELLATION_VALUE 4

#define ENABLE_FILTER_FUNCTION 1

#define MAX_EDGE_LEVEL 128.0f
#define MIN_EDGE_LEVEL   4.0f
#define LEVEL_FACTOR    64.0f
#define MAX_PATH_LENGTH  8

bool g_subdiv_mode = false;
unsigned int keyframeID = 0;

struct BRDF
{
  float Ns;               /*< specular exponent */
  float Ni;               /*< optical density for the surface (index of refraction) */
  Vec3fa Ka;              /*< ambient reflectivity */
  Vec3fa Kd;              /*< diffuse reflectivity */
  Vec3fa Ks;              /*< specular reflectivity */
  Vec3fa Kt;              /*< transmission filter */
  float dummy[30];
};

struct Medium
{
  Vec3fa transmission; //!< Transmissivity of medium.
  float eta;             //!< Refraction index of medium.
};

inline Medium make_Medium(const Vec3fa& transmission, const float eta)
{
  Medium m;
  m.transmission = transmission;
  m.eta = eta;
  return m;
}

inline Medium make_Medium_Vacuum() {
  return make_Medium(Vec3fa((float)1.0f),1.0f);
}

inline bool eq(const Medium& a, const Medium& b) {
  return (a.eta == b.eta) && eq(a.transmission, b.transmission);
}

inline Vec3fa sample_component2(const Vec3fa& c0, const Sample3f& wi0, const Medium& medium0,
                               const Vec3fa& c1, const Sample3f& wi1, const Medium& medium1,
                               const Vec3fa& Lw, Sample3f& wi_o, Medium& medium_o, const float s)
{
  const Vec3fa m0 = Lw*c0/wi0.pdf;
  const Vec3fa m1 = Lw*c1/wi1.pdf;

  const float C0 = wi0.pdf == 0.0f ? 0.0f : max(max(m0.x,m0.y),m0.z);
  const float C1 = wi1.pdf == 0.0f ? 0.0f : max(max(m1.x,m1.y),m1.z);
  const float C  = C0 + C1;

  if (C == 0.0f) {
    wi_o = make_Sample3f(Vec3fa(0,0,0),0);
    return Vec3fa(0,0,0);
  }

  const float CP0 = C0/C;
  const float CP1 = C1/C;
  if (s < CP0) {
    wi_o = make_Sample3f(wi0.v,wi0.pdf*CP0);
    medium_o = medium0; return c0;
  }
  else {
    wi_o = make_Sample3f(wi1.v,wi1.pdf*CP1);
    medium_o = medium1; return c1;
  }
}

/*! Cosine weighted hemisphere sampling. Up direction is provided as argument. */
inline Sample3f cosineSampleHemisphere(const float  u, const float  v, const Vec3fa& N)
{
  Vec3fa localDir = cosineSampleHemisphere(Vec2f(u,v));
  Sample3f s;
  s.v = frame(N) * localDir;
  s.pdf = cosineSampleHemispherePDF(localDir);
  return s;
}

////////////////////////////////////////////////////////////////////////////////
//                          Minneart BRDF                                     //
////////////////////////////////////////////////////////////////////////////////

struct Minneart
{
  /*! The reflectance parameter. The vale 0 means no reflection,
   *  and 1 means full reflection. */
  Vec3fa R;

  /*! The amount of backscattering. A value of 0 means lambertian
   *  diffuse, and inf means maximum backscattering. */
  float b;
};

inline Vec3fa Minneart__eval(const Minneart* This,
                     const Vec3fa &wo, const DifferentialGeometry &dg, const Vec3fa &wi)
{
  const float cosThetaI = clamp(dot(wi,dg.Ns));
  const float backScatter = powf(clamp(dot(wo,wi)), This->b);
  return (backScatter * cosThetaI * float(one_over_pi)) * This->R;
}

inline Vec3fa Minneart__sample(const Minneart* This,
                       const Vec3fa &wo,
                       const DifferentialGeometry &dg,
                       Sample3f &wi,
                       const Vec2f &s)
{
  wi = cosineSampleHemisphere(s.x,s.y,dg.Ns);
  return Minneart__eval(This, wo, dg, wi.v);
}

inline void Minneart__Constructor(Minneart* This, const Vec3fa& R, const float b)
{
  This->R = R;
  This->b = b;
}

inline Minneart make_Minneart(const Vec3fa& R, const float f) {
  Minneart m; Minneart__Constructor(&m,R,f); return m;
}

////////////////////////////////////////////////////////////////////////////////
//                            Velvet BRDF                                     //
////////////////////////////////////////////////////////////////////////////////

struct Velvety
{
  BRDF base;

  /*! The reflectance parameter. The vale 0 means no reflection,
   *  and 1 means full reflection. */
  Vec3fa R;

  /*! The falloff of horizon scattering. 0 no falloff,
   *  and inf means maximum falloff. */
  float f;
};

inline Vec3fa Velvety__eval(const Velvety* This,
                    const Vec3fa &wo, const DifferentialGeometry &dg, const Vec3fa &wi)
{
  const float cosThetaO = clamp(dot(wo,dg.Ns));
  const float cosThetaI = clamp(dot(wi,dg.Ns));
  const float sinThetaO = sqrt(1.0f - cosThetaO * cosThetaO);
  const float horizonScatter = powf(sinThetaO, This->f);
  return (horizonScatter * cosThetaI * float(one_over_pi)) * This->R;
}

inline Vec3fa Velvety__sample(const Velvety* This,
                      const Vec3fa &wo,
                      const DifferentialGeometry &dg,
                      Sample3f &wi,
                      const Vec2f &s)
{
  wi = cosineSampleHemisphere(s.x,s.y,dg.Ns);
  return Velvety__eval(This, wo, dg, wi.v);
}

inline void Velvety__Constructor(Velvety* This, const Vec3fa& R, const float f)
{
  This->R = R;
  This->f = f;
}

inline Velvety make_Velvety(const Vec3fa& R, const float f) {
  Velvety m; Velvety__Constructor(&m,R,f); return m;
}

////////////////////////////////////////////////////////////////////////////////
//                  Dielectric Reflection BRDF                                //
////////////////////////////////////////////////////////////////////////////////

struct DielectricReflection
{
  float eta;
};

inline Vec3fa DielectricReflection__eval(const DielectricReflection* This, const Vec3fa &wo, const DifferentialGeometry &dg, const Vec3fa &wi) {
  return Vec3fa(0.f);
}

inline Vec3fa DielectricReflection__sample(const DielectricReflection* This, const Vec3fa &wo, const DifferentialGeometry &dg, Sample3f &wi, const Vec2f &s)
{
  const float cosThetaO = clamp(dot(wo,dg.Ns));
  wi = make_Sample3f(reflect(wo,dg.Ns,cosThetaO),1.0f);
  return Vec3fa(fresnelDielectric(cosThetaO,This->eta));
}

inline void DielectricReflection__Constructor(DielectricReflection* This,
                                              const float etai,
                                              const float etat)
{
  This->eta = etai*rcp(etat);
}

inline DielectricReflection make_DielectricReflection(const float etai, const float etat) {
  DielectricReflection v; DielectricReflection__Constructor(&v,etai,etat); return v;
}

////////////////////////////////////////////////////////////////////////////////
//                                Lambertian BRDF                             //
////////////////////////////////////////////////////////////////////////////////

struct Lambertian
{
  Vec3fa R;
};

inline Vec3fa Lambertian__eval(const Lambertian* This,
                              const Vec3fa &wo, const DifferentialGeometry &dg, const Vec3fa &wi)
{
  return This->R * (1.0f/(float)(float(pi))) * clamp(dot(wi,dg.Ns));
}

inline Vec3fa Lambertian__sample(const Lambertian* This,
                                const Vec3fa &wo,
                                const DifferentialGeometry &dg,
                                Sample3f &wi,
                                const Vec2f &s)
{
  wi = cosineSampleHemisphere(s.x,s.y,dg.Ns);
  return Lambertian__eval(This, wo, dg, wi.v);
}

inline void Lambertian__Constructor(Lambertian* This, const Vec3fa& R)
{
  This->R = R;
}

inline Lambertian make_Lambertian(const Vec3fa& R) {
  Lambertian v; Lambertian__Constructor(&v,R); return v;
}


////////////////////////////////////////////////////////////////////////////////
//              Lambertian BRDF with Dielectric Layer on top                  //
////////////////////////////////////////////////////////////////////////////////

struct DielectricLayerLambertian
{
  Vec3fa T;             //!< Transmission coefficient of dielectricum
  float etait;         //!< Relative refraction index etai/etat of both media
  float etati;         //!< relative refraction index etat/etai of both media
  Lambertian ground;   //!< the BRDF of the ground layer
};

inline Vec3fa DielectricLayerLambertian__eval(const DielectricLayerLambertian* This,
                                             const Vec3fa &wo, const DifferentialGeometry &dg, const Vec3fa &wi)
{
  const float cosThetaO = dot(wo,dg.Ns);
  const float cosThetaI = dot(wi,dg.Ns);
  if (cosThetaI <= 0.0f || cosThetaO <= 0.0f) return Vec3fa(0.f);

  float cosThetaO1;
  const Sample3f wo1 = refract(wo,dg.Ns,This->etait,cosThetaO,cosThetaO1);
  float cosThetaI1;
  const Sample3f wi1 = refract(wi,dg.Ns,This->etait,cosThetaI,cosThetaI1);
  const float Fi = 1.0f - fresnelDielectric(cosThetaI,cosThetaI1,This->etait);
  const Vec3fa Fg = Lambertian__eval(&This->ground,neg(wo1.v),dg,neg(wi1.v));
  const float Fo = 1.0f - fresnelDielectric(cosThetaO,cosThetaO1,This->etait);
  return Fo * This->T * Fg * This->T * Fi;
}

inline Vec3fa DielectricLayerLambertian__sample(const DielectricLayerLambertian* This,
                                               const Vec3fa &wo,
                                               const DifferentialGeometry &dg,
                                               Sample3f &wi,
                                               const Vec2f &s)
{
  /*! refract ray into medium */
  float cosThetaO = dot(wo,dg.Ns);
  if (cosThetaO <= 0.0f) { wi = make_Sample3f(Vec3fa(0.0f),0.0f); return Vec3fa(0.f); }
  float cosThetaO1; Sample3f wo1 = refract(wo,dg.Ns,This->etait,cosThetaO,cosThetaO1);

  /*! sample ground BRDF */
  Sample3f wi1 = make_Sample3f(Vec3fa(0.f),1.f);
  Vec3fa Fg = Lambertian__sample(&This->ground,neg(wo1.v),dg,wi1,s);

  /*! refract ray out of medium */
  float cosThetaI1 = dot(wi1.v,dg.Ns);
  if (cosThetaI1 <= 0.0f) { wi = make_Sample3f(Vec3fa(0.0f),0.0f); return Vec3fa(0.f); }

  float cosThetaI;
  Sample3f wi0 = refract(neg(wi1.v),neg(dg.Ns),This->etati,cosThetaI1,cosThetaI);
  if (wi0.pdf == 0.0f) { wi = make_Sample3f(Vec3fa(0.0f),0.0f); return Vec3fa(0.f); }

  /*! accumulate contribution of path */
  wi = make_Sample3f(wi0.v,wi1.pdf);
  float Fi = 1.0f - fresnelDielectric(cosThetaI,cosThetaI1,This->etait);
  float Fo = 1.0f - fresnelDielectric(cosThetaO,cosThetaO1,This->etait);
  return Fo * This->T * Fg * This->T * Fi;
}

inline void DielectricLayerLambertian__Constructor(DielectricLayerLambertian* This,
                                                   const Vec3fa& T,
                                                   const float etai,
                                                   const float etat,
                                                   const Lambertian& ground)
{
  This->T = T;
  This->etait = etai*rcp(etat);
  This->etati = etat*rcp(etai);
  This->ground = ground;
}

inline DielectricLayerLambertian make_DielectricLayerLambertian(const Vec3fa& T,
                                                                        const float etai,
                                                                        const float etat,
                                                                        const Lambertian& ground)
{
  DielectricLayerLambertian m;
  DielectricLayerLambertian__Constructor(&m,T,etai,etat,ground);
  return m;
}

/*! Anisotropic power cosine microfacet distribution. */
struct AnisotropicBlinn {
  Vec3fa dx;       //!< x-direction of the distribution.
  Vec3fa dy;       //!< y-direction of the distribution.
  Vec3fa dz;       //!< z-direction of the distribution.
  Vec3fa Kr,Kt;
  float nx;        //!< Glossiness in x direction with range [0,infinity[ where 0 is a diffuse surface.
  float ny;        //!< Exponent that determines the glossiness in y direction.
  float norm1;     //!< Normalization constant for calculating the pdf for sampling.
  float norm2;     //!< Normalization constant for calculating the distribution.
  float side;
};

  /*! Anisotropic power cosine distribution constructor. */
inline void AnisotropicBlinn__Constructor(AnisotropicBlinn* This, const Vec3fa& Kr, const Vec3fa& Kt,
                                          const Vec3fa& dx, float nx, const Vec3fa& dy, float ny, const Vec3fa& dz)
{
  This->Kr = Kr;
  This->Kt = Kt;
  This->dx = dx;
  This->nx = nx;
  This->dy = dy;
  This->ny = ny;
  This->dz = dz;
  This->norm1 = sqrtf((nx+1)*(ny+1)) * float(one_over_two_pi);
  This->norm2 = sqrtf((nx+2)*(ny+2)) * float(one_over_two_pi);
  This->side = reduce_max(Kr)/(reduce_max(Kr)+reduce_max(Kt));
}

/*! Evaluates the power cosine distribution. \param wh is the half
 *  vector */
inline float AnisotropicBlinn__eval(const AnisotropicBlinn* This, const Vec3fa& wh)
{
  const float cosPhiH   = dot(wh, This->dx);
  const float sinPhiH   = dot(wh, This->dy);
  const float cosThetaH = dot(wh, This->dz);
  const float R = sqr(cosPhiH)+sqr(sinPhiH);
  if (R == 0.0f) return This->norm2;
  const float n = (This->nx*sqr(cosPhiH)+This->ny*sqr(sinPhiH))*rcp(R);
  return This->norm2 * powf(abs(cosThetaH), n);
}

/*! Samples the distribution. \param s is the sample location
 *  provided by the caller. */
inline Vec3fa AnisotropicBlinn__sample(const AnisotropicBlinn* This, const float sx, const float sy)
{
  const float phi =float(two_pi)*sx;
  const float sinPhi0 = sqrtf(This->nx+1)*sinf(phi);
  const float cosPhi0 = sqrtf(This->ny+1)*cosf(phi);
  const float norm = rsqrt(sqr(sinPhi0)+sqr(cosPhi0));
  const float sinPhi = sinPhi0*norm;
  const float cosPhi = cosPhi0*norm;
  const float n = This->nx*sqr(cosPhi)+This->ny*sqr(sinPhi);
  const float cosTheta = powf(sy,rcp(n+1));
  const float sinTheta = cos2sin(cosTheta);
  const float pdf = This->norm1*powf(cosTheta,n);
  const Vec3fa h = Vec3fa(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
  const Vec3fa wh = h.x*This->dx + h.y*This->dy + h.z*This->dz;
  return Vec3fa(wh,pdf);
}

inline Vec3fa AnisotropicBlinn__eval(const AnisotropicBlinn* This, const Vec3fa& wo, const Vec3fa& wi)
{
  const float cosThetaI = dot(wi,This->dz);

  /* reflection */
  if (cosThetaI > 0.0f) {
    const Vec3fa wh = normalize(wi + wo);
    return This->Kr * AnisotropicBlinn__eval(This,wh) * abs(cosThetaI);
  }

  /* transmission */
  else {
    const Vec3fa wh = normalize(reflect(wi,This->dz) + wo);
    return This->Kt * AnisotropicBlinn__eval(This,wh) * abs(cosThetaI);
  }
}

inline Vec3fa AnisotropicBlinn__sample(const AnisotropicBlinn* This, const Vec3fa& wo, Sample3f& wi_o, const float sx, const float sy, const float sz)
{
  //wi = Vec3fa(reflect(normalize(wo),normalize(dz)),1.0f); return Kr;
  //wi = Vec3fa(neg(wo),1.0f); return Kt;
  const Vec3fa wh = AnisotropicBlinn__sample(This,sx,sy);
  //if (dot(wo,wh) < 0.0f) return Vec3fa(0.0f,0.0f);

  /* reflection */
  if (sz < This->side) {
    wi_o = make_Sample3f(reflect(wo,Vec3fa(wh)),wh.w*This->side);
    const float cosThetaI = dot(wi_o.v,This->dz);
    return This->Kr * AnisotropicBlinn__eval(This,Vec3fa(wh)) * abs(cosThetaI);
  }

  /* transmission */
  else {
    wi_o = make_Sample3f(reflect(reflect(wo,Vec3fa(wh)),This->dz),wh.w*(1-This->side));
    const float cosThetaI = dot(wi_o.v,This->dz);
    return This->Kt * AnisotropicBlinn__eval(This,Vec3fa(wh)) * abs(cosThetaI);
  }
}

////////////////////////////////////////////////////////////////////////////////
//                          Matte Material                                    //
////////////////////////////////////////////////////////////////////////////////

void MatteMaterial__preprocess(MatteMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
}

Vec3fa MatteMaterial__eval(MatteMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi)
{
  Lambertian lambertian = make_Lambertian(Vec3fa((Vec3fa)This->reflectance));
  return Lambertian__eval(&lambertian,wo,dg,wi);
}

Vec3fa MatteMaterial__sample(MatteMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  Lambertian lambertian = make_Lambertian(Vec3fa((Vec3fa)This->reflectance));
  return Lambertian__sample(&lambertian,wo,dg,wi_o,s);
}

////////////////////////////////////////////////////////////////////////////////
//                          Mirror Material                                    //
////////////////////////////////////////////////////////////////////////////////

void MirrorMaterial__preprocess(MirrorMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
}

Vec3fa MirrorMaterial__eval(MirrorMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi) {
  return Vec3fa(0.0f);
}

Vec3fa MirrorMaterial__sample(MirrorMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  wi_o = make_Sample3f(reflect(wo,dg.Ns),1.0f);
  return Vec3fa(This->reflectance);
}

////////////////////////////////////////////////////////////////////////////////
//                          OBJ Material                                      //
////////////////////////////////////////////////////////////////////////////////

void OBJMaterial__preprocess(OBJMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
    float d = material->d;
    if (material->map_d) d *= getTextureTexel1f(material->map_d,dg.u,dg.v);
    brdf.Ka = Vec3fa(material->Ka);
    //if (material->map_Ka) { brdf.Ka *= material->map_Ka->get(dg.st); }
    brdf.Kd = d * Vec3fa(material->Kd);
    if (material->map_Kd) brdf.Kd = brdf.Kd * getTextureTexel3f(material->map_Kd,dg.u,dg.v);
    brdf.Ks = d * Vec3fa(material->Ks);
    //if (material->map_Ks) brdf.Ks *= material->map_Ks->get(dg.st);
    brdf.Ns = material->Ns;
    //if (material->map_Ns) { brdf.Ns *= material->map_Ns.get(dg.st); }
    brdf.Kt = (1.0f-d)*Vec3fa(material->Kt);
    brdf.Ni = material->Ni;
}

Vec3fa OBJMaterial__eval(OBJMaterial* material, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi)
{
  Vec3fa R = Vec3fa(0.0f);
  const float Md = max(max(brdf.Kd.x,brdf.Kd.y),brdf.Kd.z);
  const float Ms = max(max(brdf.Ks.x,brdf.Ks.y),brdf.Ks.z);
  const float Mt = max(max(brdf.Kt.x,brdf.Kt.y),brdf.Kt.z);
  if (Md > 0.0f) {
    R = R + (1.0f/float(pi)) * clamp(dot(wi,dg.Ns)) * brdf.Kd;
  }
  if (Ms > 0.0f) {
    const Sample3f refl = make_Sample3f(reflect(wo,dg.Ns),1.0f);
    if (dot(refl.v,wi) > 0.0f)
      R = R + (brdf.Ns+2) * float(one_over_two_pi) * powf(max(1e-10f,dot(refl.v,wi)),brdf.Ns) * clamp(dot(wi,dg.Ns)) * brdf.Ks;
  }
  if (Mt > 0.0f) {
  }
  return R;
}

Vec3fa OBJMaterial__sample(OBJMaterial* material, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  Vec3fa cd = Vec3fa(0.0f);
  Sample3f wid = make_Sample3f(Vec3fa(0.0f),0.0f);
  if (max(max(brdf.Kd.x,brdf.Kd.y),brdf.Kd.z) > 0.0f) {
    wid = cosineSampleHemisphere(s.x,s.y,dg.Ns);
    cd = float(one_over_pi) * clamp(dot(wid.v,dg.Ns)) * brdf.Kd;
  }

  Vec3fa cs = Vec3fa(0.0f);
  Sample3f wis = make_Sample3f(Vec3fa(0.0f),0.0f);
  if (max(max(brdf.Ks.x,brdf.Ks.y),brdf.Ks.z) > 0.0f)
  {
    const Sample3f refl = make_Sample3f(reflect(wo,dg.Ns),1.0f);
    wis.v = powerCosineSampleHemisphere(brdf.Ns,s);
    wis.pdf = powerCosineSampleHemispherePDF(wis.v,brdf.Ns);
    wis.v = frame(refl.v) * wis.v;
    cs = (brdf.Ns+2) * float(one_over_two_pi) * powf(max(dot(refl.v,wis.v),1e-10f),brdf.Ns) * clamp(dot(wis.v,dg.Ns)) * brdf.Ks;
  }

  Vec3fa ct = Vec3fa(0.0f);
  Sample3f wit = make_Sample3f(Vec3fa(0.0f),0.0f);
  if (max(max(brdf.Kt.x,brdf.Kt.y),brdf.Kt.z) > 0.0f)
  {
    wit = make_Sample3f(neg(wo),1.0f);
    ct = brdf.Kt;
  }

  const Vec3fa md = Lw*cd/wid.pdf;
  const Vec3fa ms = Lw*cs/wis.pdf;
  const Vec3fa mt = Lw*ct/wit.pdf;

  const float Cd = wid.pdf == 0.0f ? 0.0f : max(max(md.x,md.y),md.z);
  const float Cs = wis.pdf == 0.0f ? 0.0f : max(max(ms.x,ms.y),ms.z);
  const float Ct = wit.pdf == 0.0f ? 0.0f : max(max(mt.x,mt.y),mt.z);
  const float C  = Cd + Cs + Ct;

  if (C == 0.0f) {
    wi_o = make_Sample3f(Vec3fa(0,0,0),0);
    return Vec3fa(0,0,0);
  }

  const float CPd = Cd/C;
  const float CPs = Cs/C;
  const float CPt = Ct/C;

  if (s.x < CPd) {
    wi_o = make_Sample3f(wid.v,wid.pdf*CPd);
    return cd;
  }
  else if (s.x < CPd + CPs)
  {
    wi_o = make_Sample3f(wis.v,wis.pdf*CPs);
    return cs;
  }
  else
  {
    wi_o = make_Sample3f(wit.v,wit.pdf*CPt);
    return ct;
  }
}

////////////////////////////////////////////////////////////////////////////////
//                        Metal Material                                      //
////////////////////////////////////////////////////////////////////////////////

void MetalMaterial__preprocess(MetalMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
}

Vec3fa MetalMaterial__eval(MetalMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi)
{
  const FresnelConductor fresnel = make_FresnelConductor(Vec3fa(This->eta),Vec3fa(This->k));
  const PowerCosineDistribution distribution = make_PowerCosineDistribution(rcp(This->roughness));

  const float cosThetaO = dot(wo,dg.Ns);
  const float cosThetaI = dot(wi,dg.Ns);
  if (cosThetaI <= 0.0f || cosThetaO <= 0.0f) return Vec3fa(0.f);
  const Vec3fa wh = normalize(wi+wo);
  const float cosThetaH = dot(wh, dg.Ns);
  const float cosTheta = dot(wi, wh); // = dot(wo, wh);
  const Vec3fa F = eval(fresnel,cosTheta);
  const float D = eval(distribution,cosThetaH);
  const float G = min(1.0f, min(2.0f * cosThetaH * cosThetaO / cosTheta,
                                2.0f * cosThetaH * cosThetaI / cosTheta));
  return (Vec3fa(This->reflectance)*F) * D * G * rcp(4.0f*cosThetaO);
}

Vec3fa MetalMaterial__sample(MetalMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  const PowerCosineDistribution distribution = make_PowerCosineDistribution(rcp(This->roughness));

  if (dot(wo,dg.Ns) <= 0.0f) { wi_o = make_Sample3f(Vec3fa(0.0f),0.0f); return Vec3fa(0.f); }
  sample(distribution,wo,dg.Ns,wi_o,s);
  if (dot(wi_o.v,dg.Ns) <= 0.0f) { wi_o = make_Sample3f(Vec3fa(0.0f),0.0f); return Vec3fa(0.f); }
  return MetalMaterial__eval(This,brdf,wo,dg,wi_o.v);
}

////////////////////////////////////////////////////////////////////////////////
//                        ReflectiveMetal Material                            //
////////////////////////////////////////////////////////////////////////////////

void ReflectiveMetalMaterial__preprocess(ReflectiveMetalMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)  {
}

Vec3fa ReflectiveMetalMaterial__eval(ReflectiveMetalMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi) {
  return Vec3fa(0.0f);
}

Vec3fa ReflectiveMetalMaterial__sample(ReflectiveMetalMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  wi_o = make_Sample3f(reflect(wo,dg.Ns),1.0f);
  return Vec3fa(This->reflectance) * fresnelConductor(dot(wo,dg.Ns),Vec3fa((Vec3fa)This->eta),Vec3fa((Vec3fa)This->k));
}

////////////////////////////////////////////////////////////////////////////////
//                        Velvet Material                                     //
////////////////////////////////////////////////////////////////////////////////

void VelvetMaterial__preprocess(VelvetMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
}

Vec3fa VelvetMaterial__eval(VelvetMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi)
{
  Minneart minneart; Minneart__Constructor(&minneart,(Vec3fa)Vec3fa(This->reflectance),This->backScattering);
  Velvety velvety; Velvety__Constructor (&velvety,Vec3fa((Vec3fa)This->horizonScatteringColor),This->horizonScatteringFallOff);
  return Minneart__eval(&minneart,wo,dg,wi) + Velvety__eval(&velvety,wo,dg,wi);
}

Vec3fa VelvetMaterial__sample(VelvetMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  Minneart minneart; Minneart__Constructor(&minneart,Vec3fa((Vec3fa)This->reflectance),This->backScattering);
  Velvety velvety; Velvety__Constructor (&velvety,Vec3fa((Vec3fa)This->horizonScatteringColor),This->horizonScatteringFallOff);

  Sample3f wi0; Vec3fa c0 = Minneart__sample(&minneart,wo,dg,wi0,s);
  Sample3f wi1; Vec3fa c1 = Velvety__sample(&velvety,wo,dg,wi1,s);
  return sample_component2(c0,wi0,medium,c1,wi1,medium,Lw,wi_o,medium,s.x);
}

////////////////////////////////////////////////////////////////////////////////
//                          Dielectric Material                               //
////////////////////////////////////////////////////////////////////////////////

void DielectricMaterial__preprocess(DielectricMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
}

Vec3fa DielectricMaterial__eval(DielectricMaterial* material, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi) {
  return Vec3fa(0.0f);
}

Vec3fa DielectricMaterial__sample(DielectricMaterial* material, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  float eta = 0.0f;
  Medium mediumOutside = make_Medium(Vec3fa((Vec3fa)material->transmissionOutside),material->etaOutside);
  Medium mediumInside  = make_Medium(Vec3fa((Vec3fa)material->transmissionInside ),material->etaInside );
  Medium mediumFront, mediumBack;
  if (eq(medium,mediumInside)) {
    eta = material->etaInside/material->etaOutside;
    mediumFront = mediumInside;
    mediumBack = mediumOutside;
  }
  else {
    eta = material->etaOutside/material->etaInside;
    mediumFront = mediumOutside;
    mediumBack = mediumInside;
  }

  float cosThetaO = clamp(dot(wo,dg.Ns));
  float cosThetaI; Sample3f wit = refract(wo,dg.Ns,eta,cosThetaO,cosThetaI);
  Sample3f wis = make_Sample3f(reflect(wo,dg.Ns),1.0f);
  float R = fresnelDielectric(cosThetaO,cosThetaI,eta);
  Vec3fa cs = Vec3fa(R);
  Vec3fa ct = Vec3fa(1.0f-R);
  return sample_component2(cs,wis,mediumFront,ct,wit,mediumBack,Lw,wi_o,medium,s.x);
}

////////////////////////////////////////////////////////////////////////////////
//                          ThinDielectric Material                               //
////////////////////////////////////////////////////////////////////////////////

void ThinDielectricMaterial__preprocess(ThinDielectricMaterial* This, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
}

Vec3fa ThinDielectricMaterial__eval(ThinDielectricMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi) {
  return Vec3fa(0.0f);
}

Vec3fa ThinDielectricMaterial__sample(ThinDielectricMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  float cosThetaO = clamp(dot(wo,dg.Ns));
  if (cosThetaO <= 0.0f) return Vec3fa(0.0f);
  float R = fresnelDielectric(cosThetaO,rcp(This->eta));
  Sample3f wit = make_Sample3f(neg(wo),1.0f);
  Sample3f wis = make_Sample3f(reflect(wo,dg.Ns),1.0f);
  Vec3fa ct = exp(Vec3fa(This->transmissionFactor)*rcp(cosThetaO))*Vec3fa(1.0f-R);
  Vec3fa cs = Vec3fa(R);
  return sample_component2(cs,wis,medium,ct,wit,medium,Lw,wi_o,medium,s.x);
}

////////////////////////////////////////////////////////////////////////////////
//                     MetallicPaint Material                                 //
////////////////////////////////////////////////////////////////////////////////

void MetallicPaintMaterial__preprocess(MetallicPaintMaterial* material, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
}

Vec3fa MetallicPaintMaterial__eval(MetallicPaintMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi)
{
  DielectricReflection reflection; DielectricReflection__Constructor(&reflection, 1.0f, This->eta);
  DielectricLayerLambertian lambertian; DielectricLayerLambertian__Constructor(&lambertian, Vec3fa((float)1.0f), 1.0f, This->eta, make_Lambertian(Vec3fa((Vec3fa)This->shadeColor)));
  return DielectricReflection__eval(&reflection,wo,dg,wi) + DielectricLayerLambertian__eval(&lambertian,wo,dg,wi);
}

Vec3fa MetallicPaintMaterial__sample(MetallicPaintMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  DielectricReflection reflection; DielectricReflection__Constructor(&reflection, 1.0f, This->eta);
  DielectricLayerLambertian lambertian; DielectricLayerLambertian__Constructor(&lambertian, Vec3fa((float)1.0f), 1.0f, This->eta, make_Lambertian(Vec3fa((Vec3fa)This->shadeColor)));
  Sample3f wi0; Vec3fa c0 = DielectricReflection__sample(&reflection,wo,dg,wi0,s);
  Sample3f wi1; Vec3fa c1 = DielectricLayerLambertian__sample(&lambertian,wo,dg,wi1,s);
  return sample_component2(c0,wi0,medium,c1,wi1,medium,Lw,wi_o,medium,s.x);
}

////////////////////////////////////////////////////////////////////////////////
//                              Hair Material                                 //
////////////////////////////////////////////////////////////////////////////////

void HairMaterial__preprocess(HairMaterial* This, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
  AnisotropicBlinn__Constructor((AnisotropicBlinn*)&brdf,Vec3fa(This->Kr),Vec3fa(This->Kt),dg.Tx,(float)This->nx,dg.Ty,(float)This->ny,dg.Ng);
}

Vec3fa HairMaterial__eval(HairMaterial* This, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi)
{
  return AnisotropicBlinn__eval((AnisotropicBlinn*)&brdf,wo,wi);
}

Vec3fa HairMaterial__sample(HairMaterial* This, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  return AnisotropicBlinn__sample((AnisotropicBlinn*)&brdf,wo,wi_o,s.x,s.y,s.x);
}

////////////////////////////////////////////////////////////////////////////////
//                              Material                                      //
////////////////////////////////////////////////////////////////////////////////

inline void Material__preprocess(ISPCMaterial* materials, unsigned int materialID, unsigned int numMaterials, BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Medium& medium)
{
  unsigned int id = materialID;
  {
    if (id < numMaterials) // FIXME: workaround for ISPC bug, location reached with empty execution mask
    {
      ISPCMaterial* material = &materials[id];

      switch (material->ty) {
      case MATERIAL_OBJ  : OBJMaterial__preprocess  ((OBJMaterial*)  material,brdf,wo,dg,medium); break;
      case MATERIAL_METAL: MetalMaterial__preprocess((MetalMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_REFLECTIVE_METAL: ReflectiveMetalMaterial__preprocess((ReflectiveMetalMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_VELVET: VelvetMaterial__preprocess((VelvetMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_DIELECTRIC: DielectricMaterial__preprocess((DielectricMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_METALLIC_PAINT: MetallicPaintMaterial__preprocess((MetallicPaintMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_MATTE: MatteMaterial__preprocess((MatteMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_MIRROR: MirrorMaterial__preprocess((MirrorMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_THIN_DIELECTRIC: ThinDielectricMaterial__preprocess((ThinDielectricMaterial*)material,brdf,wo,dg,medium); break;
      case MATERIAL_HAIR: HairMaterial__preprocess((HairMaterial*)material,brdf,wo,dg,medium); break;
      default: break;
      }
    }
  }
}

inline Vec3fa Material__eval(ISPCMaterial* materials, unsigned int materialID, unsigned int numMaterials, const BRDF& brdf, const Vec3fa& wo, const DifferentialGeometry& dg, const Vec3fa& wi)
{
  Vec3fa c = Vec3fa(0.0f);
  unsigned int id = materialID;
  {
    if (id < numMaterials) // FIXME: workaround for ISPC bug, location reached with empty execution mask
    {
      ISPCMaterial* material = &materials[id];
      switch (material->ty) {
      case MATERIAL_OBJ  : c = OBJMaterial__eval  ((OBJMaterial*)  material, brdf, wo, dg, wi); break;
      case MATERIAL_METAL: c = MetalMaterial__eval((MetalMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_REFLECTIVE_METAL: c = ReflectiveMetalMaterial__eval((ReflectiveMetalMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_VELVET: c = VelvetMaterial__eval((VelvetMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_DIELECTRIC: c = DielectricMaterial__eval((DielectricMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_METALLIC_PAINT: c = MetallicPaintMaterial__eval((MetallicPaintMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_MATTE: c = MatteMaterial__eval((MatteMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_MIRROR: c = MirrorMaterial__eval((MirrorMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_THIN_DIELECTRIC: c = ThinDielectricMaterial__eval((ThinDielectricMaterial*)material, brdf, wo, dg, wi); break;
      case MATERIAL_HAIR: c = HairMaterial__eval((HairMaterial*)material, brdf, wo, dg, wi); break;
      default: c = Vec3fa(0.0f);
      }
    }
  }
  return c;
}

inline Vec3fa Material__sample(ISPCMaterial* materials, unsigned int materialID, unsigned int numMaterials, const BRDF& brdf, const Vec3fa& Lw, const Vec3fa& wo, const DifferentialGeometry& dg, Sample3f& wi_o, Medium& medium, const Vec2f& s)
{
  Vec3fa c = Vec3fa(0.0f);
  unsigned int id = materialID;
  {
    if (id < numMaterials) // FIXME: workaround for ISPC bug, location reached with empty execution mask
    {
      ISPCMaterial* material = &materials[id];
      switch (material->ty) {
      case MATERIAL_OBJ  : c = OBJMaterial__sample  ((OBJMaterial*)  material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_METAL: c = MetalMaterial__sample((MetalMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_REFLECTIVE_METAL: c = ReflectiveMetalMaterial__sample((ReflectiveMetalMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_VELVET: c = VelvetMaterial__sample((VelvetMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_DIELECTRIC: c = DielectricMaterial__sample((DielectricMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_METALLIC_PAINT: c = MetallicPaintMaterial__sample((MetallicPaintMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_MATTE: c = MatteMaterial__sample((MatteMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_MIRROR: c = MirrorMaterial__sample((MirrorMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_THIN_DIELECTRIC: c = ThinDielectricMaterial__sample((ThinDielectricMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      case MATERIAL_HAIR: c = HairMaterial__sample((HairMaterial*)material, brdf, Lw, wo, dg, wi_o, medium, s); break;
      default: wi_o = make_Sample3f(Vec3fa(0.0f),0.0f); c = Vec3fa(0.0f); break;
      }
    }
  }
  return c;
}


////////////////////////////////////////////////////////////////////////////////
//                               Scene                                        //
////////////////////////////////////////////////////////////////////////////////

/* scene data */
extern "C" ISPCScene* g_ispc_scene;
RTCDevice g_device = nullptr;
RTCScene g_scene = nullptr;
extern "C" RTCScene* geomID_to_scene;
extern "C" ISPCInstance** geomID_to_inst;

/* occlusion filter function */
void intersectionFilterReject(void* ptr, RTCRay& ray);
void intersectionFilterOBJ(void* ptr, RTCRay& ray);
void occlusionFilterOpaque(void* ptr, RTCRay& ray);
void occlusionFilterOBJ(void* ptr, RTCRay& ray);
void occlusionFilterHair(void* ptr, RTCRay& ray);

/* accumulation buffer */
Vec3fa* g_accu = nullptr;
unsigned int g_accu_width = 0;
unsigned int g_accu_height = 0;
unsigned int g_accu_count = 0;
Vec3fa g_accu_vx;
Vec3fa g_accu_vy;
Vec3fa g_accu_vz;
Vec3fa g_accu_p;
extern "C" bool g_changed;
extern "C" int g_instancing_mode;


bool g_animation = true;
bool g_use_smooth_normals = false;
void device_key_pressed_handler(int key)
{
  if (key == 32  /* */) g_animation = !g_animation;
  if (key == 115 /*s*/) { g_use_smooth_normals = !g_use_smooth_normals; g_changed = true; }
  else device_key_pressed_default(key);
}

void assignShaders(ISPCGeometry* geometry)
{
  if (geometry->type == SUBDIV_MESH) {
    ISPCSubdivMesh* mesh = (ISPCSubdivMesh* ) geometry;
#if ENABLE_FILTER_FUNCTION == 1
    rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterOpaque);
#endif
  }
  else if (geometry->type == TRIANGLE_MESH) {
    ISPCTriangleMesh* mesh = (ISPCTriangleMesh* ) geometry;
#if ENABLE_FILTER_FUNCTION == 1
    rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterOpaque);
    
    ISPCMaterial& material = g_ispc_scene->materials[mesh->materialID];
    //if (material.ty == MATERIAL_DIELECTRIC || material.ty == MATERIAL_THIN_DIELECTRIC)
    //  rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&intersectionFilterReject);
    //else
    if (material.ty == MATERIAL_OBJ)
    {
      OBJMaterial& obj = (OBJMaterial&) material;
      if (obj.d != 1.0f || obj.map_d) {
        rtcSetIntersectionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&intersectionFilterOBJ);
        rtcSetOcclusionFilterFunction   (mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterOBJ);
      }
    }
#endif
  }
  else if (geometry->type == QUAD_MESH) {
    ISPCQuadMesh* mesh = (ISPCQuadMesh*) geometry;
#if ENABLE_FILTER_FUNCTION == 1
    rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterOpaque);
    
    ISPCMaterial& material = g_ispc_scene->materials[mesh->materialID];
    //if (material.ty == MATERIAL_DIELECTRIC || material.ty == MATERIAL_THIN_DIELECTRIC)
    //  rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&intersectionFilterReject);
    //else
    if (material.ty == MATERIAL_OBJ)
    {
      OBJMaterial& obj = (OBJMaterial&) material;
      if (obj.d != 1.0f || obj.map_d) {
        rtcSetIntersectionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&intersectionFilterOBJ);
        rtcSetOcclusionFilterFunction   (mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterOBJ);
      }
    }
#endif
  }
  else if (geometry->type == LINE_SEGMENTS) {
    ISPCLineSegments* mesh = (ISPCLineSegments*) geometry;
    rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterHair);
  }
  else if (geometry->type == HAIR_SET) {
    ISPCHairSet* mesh = (ISPCHairSet*) geometry;
    rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterHair);
  }
  else if (geometry->type == CURVES) {
    ISPCHairSet* mesh = (ISPCHairSet*) geometry;
    rtcSetOcclusionFilterFunction(mesh->scene,mesh->geomID,(RTCFilterFunc)&occlusionFilterHair);
  }
  else if (geometry->type == GROUP) {
    ISPCGroup* group = (ISPCGroup*) geometry;
    for (size_t i=0; i<group->numGeometries; i++)
      assignShaders(group->geometries[i]);
  }
}

typedef ISPCInstance* ISPCInstance_ptr;
typedef ISPCGeometry* ISPCGeometry_ptr;

RTCScene convertScene(ISPCScene* scene_in)
{
  for (size_t i=0; i<scene_in->numGeometries; i++)
  {
    ISPCGeometry* geometry = scene_in->geometries[i];
    if (geometry->type == SUBDIV_MESH) {
      g_subdiv_mode = true; break;
    }
  }

  /* create scene */
  int scene_flags = RTC_SCENE_STATIC | RTC_SCENE_INCOHERENT;
  int scene_aflags = RTC_INTERSECT1;

  if (g_subdiv_mode)
    scene_flags = RTC_SCENE_DYNAMIC | RTC_SCENE_INCOHERENT | RTC_SCENE_ROBUST;

  scene_aflags |= RTC_INTERPOLATE;

  RTCScene scene_out = ConvertScene(g_device, g_ispc_scene,(RTCSceneFlags)scene_flags, (RTCAlgorithmFlags) scene_aflags, RTC_GEOMETRY_STATIC);

  /* assign shaders */
  for (unsigned int i=0; i<scene_in->numGeometries; i++) {
    assignShaders(scene_in->geometries[i]);
  }

  /* commit individual objects in case of instancing */
  if (g_instancing_mode == 2 || g_instancing_mode == 3) 
  {
    for (unsigned int i=0; i<scene_in->numGeometries; i++) {
      if (geomID_to_scene[i]) rtcCommit(geomID_to_scene[i]);
    }
  }

  /* commit changes to scene */
  progressStart();
  rtcSetProgressMonitorFunction(scene_out,(RTCProgressMonitorFunc)&progressMonitor,nullptr);
  rtcCommit (scene_out);
  rtcSetProgressMonitorFunction(scene_out,nullptr,nullptr);
  progressEnd();

  return scene_out;
} // convertScene

inline Vec3fa face_forward(const Vec3fa& dir, const Vec3fa& _Ng) {
  const Vec3fa Ng = _Ng;
  return dot(dir,Ng) < 0.0f ? Ng : neg(Ng);
}

inline void evalBezier(const ISPCHairSet* hair, const int primID, const float t, Vec3fa& p, Vec3fa& dp)
{
  const float t0 = 1.0f - t, t1 = t;
  const Vec3fa* vertices = hair->positions;
  const ISPCHair* hairs = hair->hairs;
  
  const int i = hairs[primID].vertex;
  const Vec3fa p00 = vertices[i+0];
  const Vec3fa p01 = vertices[i+1];
  const Vec3fa p02 = vertices[i+2];
  const Vec3fa p03 = vertices[i+3];

  const Vec3fa p10 = p00 * t0 + p01 * t1;
  const Vec3fa p11 = p01 * t0 + p02 * t1;
  const Vec3fa p12 = p02 * t0 + p03 * t1;
  const Vec3fa p20 = p10 * t0 + p11 * t1;
  const Vec3fa p21 = p11 * t0 + p12 * t1;
  const Vec3fa p30 = p20 * t0 + p21 * t1;

  p = p30;
  dp = 3.0f*(p21-p20);
}

void postIntersectGeometry(const RTCRay& ray, DifferentialGeometry& dg, ISPCGeometry* geometry, int& materialID)
{
  if (geometry->type == TRIANGLE_MESH)
  {
    ISPCTriangleMesh* mesh = (ISPCTriangleMesh*) geometry;
    materialID = mesh->triangles[ray.primID].materialID;
    if (mesh->texcoords) {
      ISPCTriangle* tri = &mesh->triangles[ray.primID];
      const Vec2f st0 = mesh->texcoords[tri->v0];
      const Vec2f st1 = mesh->texcoords[tri->v1];
      const Vec2f st2 = mesh->texcoords[tri->v2];
      const float u = ray.u, v = ray.v, w = 1.0f-ray.u-ray.v;
      const Vec2f st = w*st0 + u*st1 + v*st2;
      dg.u = st.x;
      dg.v = st.y;
      /*
      const Vec3fa n0 = mesh->normals[tri->v0];
      const Vec3fa n1 = mesh->normals[tri->v1];
      const Vec3fa n2 = mesh->normals[tri->v2];
      dg.Ns = w*n0 + u*n1 + v*n2;
      */
    }
  }
  else if (geometry->type == QUAD_MESH)
  {
    ISPCQuadMesh* mesh = (ISPCQuadMesh*) geometry;
    materialID = mesh->materialID;
    if (mesh->texcoords) {
      ISPCQuad* quad = &mesh->quads[ray.primID];
      const Vec2f st0 = mesh->texcoords[quad->v0];
      const Vec2f st1 = mesh->texcoords[quad->v1];
      const Vec2f st2 = mesh->texcoords[quad->v2];
      const Vec2f st3 = mesh->texcoords[quad->v3];
      if (ray.u+ray.v < 1.0f) {
        const float u = ray.u, v = ray.v; const float w = 1.0f-u-v;
        const Vec2f st = w*st0 + u*st1 + v*st3;
        dg.u = st.x;
        dg.v = st.y;
      } else {
        const float u = 1.0f-ray.u, v = 1.0f-ray.v; const float w = 1.0f-u-v;
        const Vec2f st = w*st2 + u*st3 + v*st1;
        dg.u = st.x;
        dg.v = st.y;
      }
    }
  }
  else if (geometry->type == SUBDIV_MESH)
  {
    ISPCSubdivMesh* mesh = (ISPCSubdivMesh*) geometry;
    materialID = mesh->materialID;
    const Vec2f st = getTextureCoordinatesSubdivMesh(mesh,ray.primID,ray.u,ray.v);
    dg.u = st.x;
    dg.v = st.y;
  }
  else if (geometry->type == LINE_SEGMENTS)
  {
    ISPCLineSegments* mesh = (ISPCLineSegments*) geometry;
    materialID = mesh->materialID;
    const Vec3fa dx = normalize(dg.Ng);
    const Vec3fa dy = normalize(cross(neg(ray.dir),dx));
    const Vec3fa dz = normalize(cross(dy,dx));
    dg.Tx = dx;
    dg.Ty = dy;
    dg.Ng = dg.Ns = dz;
    int vtx = mesh->indices[ray.primID];
    dg.tnear_eps = 1.1f*mesh->positions[vtx].w;
  }
  else if (geometry->type == HAIR_SET)
  {
    ISPCHairSet* mesh = (ISPCHairSet*) geometry;
    materialID = mesh->materialID;
    Vec3fa p,dp; evalBezier(mesh,ray.primID,ray.u,p,dp);
    const Vec3fa dx = normalize(dg.Ng);
    const Vec3fa dy = normalize(cross(neg(ray.dir),dx));
    const Vec3fa dz = normalize(cross(dy,dx));
    dg.Tx = dx;
    dg.Ty = dy;
    dg.Ng = dg.Ns = dz;
    dg.tnear_eps = 1.1f*p.w;
  }
  else if (geometry->type == CURVES)
  {
    ISPCHairSet* mesh = (ISPCHairSet*) geometry;
    materialID = mesh->materialID;
    Vec3fa p,dp; evalBezier(mesh,ray.primID,ray.u,p,dp);
    if (length(dp) < 1E-6f) { // some hair are just points
      dg.Tx = Vec3fa(1,0,0);
      dg.Ty = Vec3fa(0,1,0);
      dg.Ng = dg.Ns = Vec3fa(0,0,1);
    }
    else
    {
      const Vec3fa dx = normalize(Vec3fa(dp));
      const Vec3fa dy = normalize(cross(Vec3fa(dp),dg.Ng));
      const Vec3fa dz = normalize(dg.Ng);
      dg.Tx = dx;
      dg.Ty = dy;
      dg.Ng = dg.Ns = dz;
    }
    dg.tnear_eps = 1024.0f*1.19209e-07f*max(max(abs(dg.P.x),abs(dg.P.y)),max(abs(dg.P.z),ray.tfar));
  }
  else if (geometry->type == GROUP) {
    unsigned int geomID = ray.geomID; {
      postIntersectGeometry(ray,dg,((ISPCGroup*) geometry)->geometries[geomID],materialID);
    }
  }
  else
    assert(false);
}

AffineSpace3fa calculate_interpolated_space (ISPCInstance* instance, float gtime)
{
  if (instance->numTimeSteps == 1) 
    return AffineSpace3fa(instance->spaces[0]);

  /* calculate time segment itime and fractional time ftime */
  const int time_segments = instance->numTimeSteps-1;
  const float time = gtime*(float)(time_segments);
  const int itime = clamp((int)(floor(time)),(int)0,time_segments-1);
  const float ftime = time - (float)(itime);
  return (1.0f-ftime)*AffineSpace3fa(instance->spaces[itime+0]) + ftime*AffineSpace3fa(instance->spaces[itime+1]);
}

inline int postIntersect(const RTCRay& ray, DifferentialGeometry& dg)
{
  int materialID = 0;
  unsigned ray_geomID = g_instancing_mode >= 2 ? ray.instID : ray.geomID;
  dg.tnear_eps = 32.0f*1.19209e-07f*max(max(abs(dg.P.x),abs(dg.P.y)),max(abs(dg.P.z),ray.tfar));
  unsigned int geomID = ray_geomID;
  {
    /* get instance and geometry pointers */
    ISPCInstance* instance;
    ISPCGeometry* geometry;
    if (g_instancing_mode) {
      instance = geomID_to_inst[geomID];
      geometry = g_ispc_scene->geometries[instance->geomID];
    } else {
      instance = nullptr;
      geometry = g_ispc_scene->geometries[geomID];
    }

    postIntersectGeometry(ray,dg,geometry,materialID);

    /* convert normals */
    if (instance) {
      //AffineSpace3fa space = (1.0f-ray.time)*AffineSpace3fa(instance->space0) + ray.time*AffineSpace3fa(instance->space1);
      AffineSpace3fa space = calculate_interpolated_space(instance,ray.time);
      dg.Ng = xfmVector(space,dg.Ng);
      dg.Ns = xfmVector(space,dg.Ns);
    }
  }

  return materialID;
}

void intersectionFilterReject(void* ptr, RTCRay& ray) {
  ray.geomID = RTC_INVALID_GEOMETRY_ID;
}

void intersectionFilterOBJ(void* ptr, RTCRay& ray)
{
  /* compute differential geometry */
  DifferentialGeometry dg;
  dg.geomID = ray.geomID;
  dg.primID = ray.primID;
  dg.u = ray.u;
  dg.v = ray.v;
  dg.P  = ray.org+ray.tfar*ray.dir;
  dg.Ng = ray.Ng;
  dg.Ns = ray.Ng;
  int materialID = postIntersect(ray,dg);
  dg.Ng = face_forward(ray.dir,normalize(dg.Ng));
  dg.Ns = face_forward(ray.dir,normalize(dg.Ns));
  const Vec3fa wo = neg(ray.dir);

  /* calculate BRDF */
  BRDF brdf; brdf.Kt = Vec3fa(0,0,0);
  int numMaterials = g_ispc_scene->numMaterials;
  ISPCMaterial* material_array = &g_ispc_scene->materials[0];
  Medium medium = make_Medium_Vacuum();
  Material__preprocess(material_array,materialID,numMaterials,brdf,wo,dg,medium);
  if (min(min(brdf.Kt.x,brdf.Kt.y),brdf.Kt.z) >= 1.0f)
    ray.geomID = RTC_INVALID_GEOMETRY_ID;
}

void occlusionFilterOpaque(void* ptr, RTCRay& ray) {
  ray.transparency = Vec3fa(0.0f);
}

void occlusionFilterOBJ(void* ptr, RTCRay& ray)
{
  /* compute differential geometry */
  DifferentialGeometry dg;
  dg.geomID = ray.geomID;
  dg.primID = ray.primID;
  dg.u = ray.u;
  dg.v = ray.v;
  dg.P  = ray.org+ray.tfar*ray.dir;
  dg.Ng = ray.Ng;
  dg.Ns = ray.Ng;
  int materialID = postIntersect(ray,dg);
  dg.Ng = face_forward(ray.dir,normalize(dg.Ng));
  dg.Ns = face_forward(ray.dir,normalize(dg.Ns));
  const Vec3fa wo = neg(ray.dir);

  /* calculate BRDF */
  BRDF brdf; brdf.Kt = Vec3fa(0,0,0);
  int numMaterials = g_ispc_scene->numMaterials;
  ISPCMaterial* material_array = &g_ispc_scene->materials[0];
  Medium medium = make_Medium_Vacuum();
  Material__preprocess(material_array,materialID,numMaterials,brdf,wo,dg,medium);

  ray.transparency = ray.transparency * brdf.Kt;
  if (max(max(ray.transparency.x,ray.transparency.y),ray.transparency.z) > 0.0f)
    ray.geomID = RTC_INVALID_GEOMETRY_ID;
}

/* occlusion filter function */
void occlusionFilterHair(void* ptr, RTCRay& ray)
{
  Vec3fa Kt = Vec3fa(0.0f);
  unsigned int geomID = ray.geomID;
  {
    ISPCGeometry* geometry = g_ispc_scene->geometries[geomID];
    if (geometry->type == LINE_SEGMENTS)
    {
      int materialID = ((ISPCLineSegments*)geometry)->materialID;
      ISPCMaterial* material = &g_ispc_scene->materials[materialID];
      switch (material->ty) {
      case MATERIAL_HAIR: Kt = Vec3fa(((HairMaterial*)material)->Kt); break;
      default: break;
      }
    }
    else if (geometry->type == HAIR_SET)
    {
      int materialID = ((ISPCHairSet*)geometry)->materialID;
      ISPCMaterial* material = &g_ispc_scene->materials[materialID];
      switch (material->ty) {
      case MATERIAL_HAIR: Kt = Vec3fa(((HairMaterial*)material)->Kt); break;
      default: break;
      }
    }
    else if (geometry->type == CURVES)
    {
      /*if (dot(ray.dir,ray.Ng) > 0.0f) {
        Kt = Vec3fa(1.0f);
      }
      else*/
      {
        int materialID = ((ISPCHairSet*)geometry)->materialID;
        ISPCMaterial* material = &g_ispc_scene->materials[materialID];
        switch (material->ty) {
        case MATERIAL_HAIR: Kt = Vec3fa(((HairMaterial*)material)->Kt); break;
        default: break;
        }
      }
    }
  }

  Kt = Kt * ray.transparency;
  ray.transparency = Kt;
  if (max(max(ray.transparency.x,ray.transparency.y),ray.transparency.z) > 0.0f)
    ray.geomID = RTC_INVALID_GEOMETRY_ID;
}

Vec3fa renderPixelFunction(float x, float y, RandomSampler& sampler, const ISPCCamera& camera)
{
  /* radiance accumulator and weight */
  Vec3fa L = Vec3fa(0.0f);
  Vec3fa Lw = Vec3fa(1.0f);
  Medium medium = make_Medium_Vacuum();
  float time = RandomSampler_get1D(sampler);

  /* initialize ray */
  RTCRay ray = RTCRay(Vec3fa(camera.xfm.p),
                        Vec3fa(normalize(x*camera.xfm.l.vx + y*camera.xfm.l.vy + camera.xfm.l.vz)),0.0f,inf,time);

  DifferentialGeometry dg;

  /* iterative path tracer loop */
  for (int i=0; i<MAX_PATH_LENGTH; i++)
  {
    /* terminate if contribution too low */
    if (max(Lw.x,max(Lw.y,Lw.z)) < 0.01f)
      break;

    /* intersect ray with scene */
    rtcIntersect(g_scene,ray);
    const Vec3fa wo = neg(ray.dir);

    /* invoke environment lights if nothing hit */
    if (ray.geomID == RTC_INVALID_GEOMETRY_ID)
    {
      //L = L + Lw*Vec3fa(1.0f);

      /* iterate over all lights */
      for (size_t i=0; i<g_ispc_scene->numLights; i++)
      {
        const Light* l = g_ispc_scene->lights[i];
        Light_EvalRes le = l->eval(l,dg,ray.dir);
        L = L + Lw*le.value;
      }

      break;
    }
    Vec3fa Ns = normalize(ray.Ng);

    if (g_use_smooth_normals)
      if (ray.geomID != RTC_INVALID_GEOMETRY_ID) // FIXME: workaround for ISPC bug, location reached with empty execution mask
    {
      Vec3fa dPdu,dPdv;
      unsigned int geomID = ray.geomID; {
        rtcInterpolate(g_scene,geomID,ray.primID,ray.u,ray.v,RTC_VERTEX_BUFFER0,nullptr,&dPdu.x,&dPdv.x,3);
      }
      Ns = normalize(cross(dPdv,dPdu));
    }

    /* compute differential geometry */
    dg.geomID = ray.geomID;
    dg.primID = ray.primID;
    dg.u = ray.u;
    dg.v = ray.v;
    dg.P  = ray.org+ray.tfar*ray.dir;
    dg.Ng = ray.Ng;
    dg.Ns = Ns;
    int materialID = postIntersect(ray,dg);
    dg.Ng = face_forward(ray.dir,normalize(dg.Ng));
    dg.Ns = face_forward(ray.dir,normalize(dg.Ns));

    /*! Compute  simple volumetric effect. */
    Vec3fa c = Vec3fa(1.0f);
    const Vec3fa transmission = medium.transmission;
    if (ne(transmission,Vec3fa(1.0f)))
      c = c * pow(transmission,ray.tfar);

    /* calculate BRDF */
    BRDF brdf;
    int numMaterials = g_ispc_scene->numMaterials;
    ISPCMaterial* material_array = &g_ispc_scene->materials[0];
    Material__preprocess(material_array,materialID,numMaterials,brdf,wo,dg,medium);

    /* sample BRDF at hit point */
    Sample3f wi1;
    c = c * Material__sample(material_array,materialID,numMaterials,brdf,Lw, wo, dg, wi1, medium, RandomSampler_get2D(sampler));

    /* iterate over lights */
    for (size_t i=0; i<g_ispc_scene->numLights; i++)
    {
      const Light* l = g_ispc_scene->lights[i];
      Light_SampleRes ls = l->sample(l,dg,RandomSampler_get2D(sampler));
      if (ls.pdf <= 0.0f) continue;
      RTCRay shadow = RTCRay(dg.P,ls.dir,dg.tnear_eps,ls.dist,time); shadow.transparency = Vec3fa(1.0f);
      rtcOccluded(g_scene,shadow);
      //if (shadow.geomID != RTC_INVALID_GEOMETRY_ID) continue;
      if (max(max(shadow.transparency.x,shadow.transparency.y),shadow.transparency.z) > 0.0f)
        L = L + Lw*ls.weight*shadow.transparency*Material__eval(material_array,materialID,numMaterials,brdf,wo,dg,ls.dir);
    }

    if (wi1.pdf <= 1E-4f /* 0.0f */) break;
    Lw = Lw*c/wi1.pdf;

    /* setup secondary ray */
    float sign = dot(wi1.v,dg.Ng) < 0.0f ? -1.0f : 1.0f;
    dg.P = dg.P + sign*dg.tnear_eps*dg.Ng;
    ray = RTCRay(dg.P,normalize(wi1.v),dg.tnear_eps,inf,time);
  }
  return L;
}

/* task that renders a single screen tile */
Vec3fa renderPixelStandard(float x, float y, const ISPCCamera& camera)
{
  RandomSampler sampler;

  Vec3fa L = Vec3fa(0.0f);

  for (int i=0; i<SAMPLES_PER_PIXEL; i++)
  {
    RandomSampler_init(sampler, (int)x, (int)y, g_accu_count*SAMPLES_PER_PIXEL+i);

    /* calculate pixel color */
    float fx = x + RandomSampler_get1D(sampler);
    float fy = y + RandomSampler_get1D(sampler);
    L = L + renderPixelFunction(fx,fy,sampler,camera);
  }
  L = L*(1.0f/SAMPLES_PER_PIXEL);
  return L;
}

/* renders a single screen tile */
void renderTileStandard(int taskIndex,
                        int* pixels,
                        const unsigned int width,
                        const unsigned int height,
                        const float time,
                        const ISPCCamera& camera,
                        const int numTilesX,
                        const int numTilesY)
{
  const unsigned int tileY = taskIndex / numTilesX;
  const unsigned int tileX = taskIndex - tileY * numTilesX;
  const unsigned int x0 = tileX * TILE_SIZE_X;
  const unsigned int x1 = min(x0+TILE_SIZE_X,width);
  const unsigned int y0 = tileY * TILE_SIZE_Y;
  const unsigned int y1 = min(y0+TILE_SIZE_Y,height);

  for (unsigned int y=y0; y<y1; y++) for (unsigned int x=x0; x<x1; x++)
  {
    /* calculate pixel color */
    Vec3fa color = renderPixelStandard((float)x,(float)y,camera);

    /* write color to framebuffer */
    Vec3fa accu_color = g_accu[y*width+x] + Vec3fa(color.x,color.y,color.z,1.0f); g_accu[y*width+x] = accu_color;
    float f = rcp(max(0.001f,accu_color.w));
    unsigned int r = (unsigned int) (255.0f * clamp(accu_color.x*f,0.0f,1.0f));
    unsigned int g = (unsigned int) (255.0f * clamp(accu_color.y*f,0.0f,1.0f));
    unsigned int b = (unsigned int) (255.0f * clamp(accu_color.z*f,0.0f,1.0f));
    pixels[y*width+x] = (b << 16) + (g << 8) + r;
  }
}

/* task that renders a single screen tile */
void renderTileTask (int taskIndex, int* pixels,
                         const unsigned int width,
                         const unsigned int height,
                         const float time,
                         const ISPCCamera& camera,
                         const int numTilesX,
                         const int numTilesY)
{
  renderTile(taskIndex,pixels,width,height,time,camera,numTilesX,numTilesY);
}


/***************************************************************************************/

inline float updateEdgeLevel( ISPCSubdivMesh* mesh, const Vec3fa& cam_pos, const size_t e0, const size_t e1)
{
  const Vec3fa v0 = mesh->positions[mesh->position_indices[e0]];
  const Vec3fa v1 = mesh->positions[mesh->position_indices[e1]];
  const Vec3fa edge = v1-v0;
  const Vec3fa P = 0.5f*(v1+v0);
  const Vec3fa dist = cam_pos - P;
  return max(min(LEVEL_FACTOR*(0.5f*length(edge)/length(dist)),MAX_EDGE_LEVEL),MIN_EDGE_LEVEL);
}

void updateEdgeLevelBuffer( ISPCSubdivMesh* mesh, const Vec3fa& cam_pos, size_t startID, size_t endID )
{
  for (size_t f=startID; f<endID;f++)
  {
    unsigned int e = mesh->face_offsets[f];
    unsigned int N = mesh->verticesPerFace[f];
    if (N == 4) /* fast path for quads */
      for (size_t i=0; i<4; i++)
        mesh->subdivlevel[e+i] =  updateEdgeLevel(mesh,cam_pos,e+(i+0),e+(i+1)%4);
       else if (N == 3) /* fast path for triangles */
         for (size_t i=0; i<3; i++)
           mesh->subdivlevel[e+i] =  updateEdgeLevel(mesh,cam_pos,e+(i+0),e+(i+1)%3);
       else /* fast path for general polygons */
         for (size_t i=0; i<N; i++)
           mesh->subdivlevel[e+i] =  updateEdgeLevel(mesh,cam_pos,e+(i+0),e+(i+1)%N);
  }
}

#if defined(ISPC)
void updateEdgeLevelBufferTask (int taskIndex,  ISPCSubdivMesh* mesh, const Vec3fa& cam_pos )
{
  const size_t size = mesh->numFaces;
  const size_t startID = ((taskIndex+0)*size)/taskCount;
  const size_t endID   = ((taskIndex+1)*size)/taskCount;
  updateEdgeLevelBuffer(mesh,cam_pos,startID,endID);
}
#endif

void updateEdgeLevels(ISPCScene* scene_in, const Vec3fa& cam_pos)
{
  for (size_t g=0; g<scene_in->numGeometries; g++)
  {
    ISPCGeometry* geometry = g_ispc_scene->geometries[g];
    if (geometry->type != SUBDIV_MESH) continue;
    ISPCSubdivMesh* mesh = (ISPCSubdivMesh*) geometry;
    unsigned int geomID = mesh->geomID;
#if defined(ISPC)
    parallel_for(size_t(0),size_t( (mesh->numFaces+4095)/4096 ),[&](const range<size_t>& range) {
    for (size_t i=range.begin(); i<range.end(); i++)
      updateEdgeLevelBufferTask((int)i,mesh,cam_pos);
  }); 
#else
      updateEdgeLevelBuffer(mesh,cam_pos,0,mesh->numFaces);
#endif
   rtcUpdateBuffer(g_scene,geomID,RTC_LEVEL_BUFFER);
  }
}

/* called by the C++ code for initialization */
extern "C" void device_init (char* cfg)
{
  /* initialize last seen camera */
  g_accu_vx = Vec3fa(0.0f);
  g_accu_vy = Vec3fa(0.0f);
  g_accu_vz = Vec3fa(0.0f);
  g_accu_p  = Vec3fa(0.0f);

  /* create new Embree device */
  g_device = rtcNewDevice(cfg);
  error_handler(rtcDeviceGetError(g_device));

  /* set error handler */
  rtcDeviceSetErrorFunction(g_device,error_handler);

  /* set start render mode */
  renderTile = renderTileStandard;
  key_pressed_handler = device_key_pressed_handler;

#if ENABLE_FILTER_FUNCTION == 0
  printf("Warning: filter functions disabled\n");
#endif

} // device_init

/* called by the C++ code to render */
extern "C" void device_render (int* pixels,
                           const unsigned int width,
                           const unsigned int height,
                           const float time,
                           const ISPCCamera& camera)
{
  /* create scene */
  if (g_scene == nullptr) {
    g_scene = convertScene(g_ispc_scene);
    if (g_subdiv_mode) updateEdgeLevels(g_ispc_scene,camera.xfm.p);
    rtcCommit (g_scene);
  }

  /* create accumulator */
  if (g_accu_width != width || g_accu_height != height) {
    alignedFree(g_accu);
    g_accu = (Vec3fa*) alignedMalloc(width*height*sizeof(Vec3fa));
    g_accu_width = width;
    g_accu_height = height;
    for (size_t i=0; i<width*height; i++)
      g_accu[i] = Vec3fa(0.0f);
  }

  /* reset accumulator */
  bool camera_changed = g_changed; g_changed = false;
  camera_changed |= ne(g_accu_vx,camera.xfm.l.vx); g_accu_vx = camera.xfm.l.vx;
  camera_changed |= ne(g_accu_vy,camera.xfm.l.vy); g_accu_vy = camera.xfm.l.vy;
  camera_changed |= ne(g_accu_vz,camera.xfm.l.vz); g_accu_vz = camera.xfm.l.vz;
  camera_changed |= ne(g_accu_p, camera.xfm.p);    g_accu_p  = camera.xfm.p;

#if  FIXED_SAMPLING == 0
  g_accu_count++;
#endif

  if (camera_changed)
  {
    g_accu_count=0;
    for (size_t i=0; i<width*height; i++)
      g_accu[i] = Vec3fa(0.0f);

    if (g_subdiv_mode) {
      updateEdgeLevels(g_ispc_scene,camera.xfm.p);
      rtcCommit (g_scene);
    }
  }

  /* render image */
  const int numTilesX = (width +TILE_SIZE_X-1)/TILE_SIZE_X;
  const int numTilesY = (height+TILE_SIZE_Y-1)/TILE_SIZE_Y;
  parallel_for(size_t(0),size_t(numTilesX*numTilesY),[&](const range<size_t>& range) {
    for (size_t i=range.begin(); i<range.end(); i++)
      renderTileTask((int)i,pixels,width,height,time,camera,numTilesX,numTilesY);
  }); 
  //rtcDebug();
} // device_render

/* called by the C++ code for cleanup */
extern "C" void device_cleanup ()
{
  rtcDeleteScene (g_scene); g_scene = nullptr;
  rtcDeleteDevice(g_device); g_device = nullptr;
  alignedFree(g_accu); g_accu = nullptr;
  g_accu_width = 0;
  g_accu_height = 0;
  g_accu_count = 0;
} // device_cleanup

} // namespace embree
