
/*

  https://learnopengl.com/PBR/Lighting

  NOTE: Definitions:

    - Radiance is the strength of light coming from a single direction (infintesimal point/line).

    - Radiant Flux: Transmitted energy of light source measured in Watts. Light is a sum of energy over various wave lengths that overlap.

    - Radiant Intensity measures the amount of radiant flux per solid angle, so the radiant flux over a area, not from a single direction.

    - Radiance is measure of light in a area scaled by angle Theta of the light to the normal of the surface by cos(theta).

    - Irradiance is the sum of all radiance from all directions.

  NOTE: Rendering Equation

    - https://www.youtube.com/watch?v=eo_MTI-d28s

    L_o(x, w0) = Le(x, w0) + Integral(angles, f_r(x, wi -> w0)*L_i(x, wi)*dot(wi, n)*dwi)

    - x is the surface point
    - w0 is the view dir from camera to surface point
    - L_o is the radiance emmitted from the surface in w0 direction (color of the surface)
    - Le is the emitted light from the surface point (emissive objects). Otherwise, this is 0
    - Integral is of all rays incoming to the surface point in a normal oriented hemisphere at x (wi being each direction)
    - f_r evaluates the BRDF at x
    - L_i is the radiance that reaches x from w_i direction
    - the dot is modulating since less light hits at grazing angles
    
    This equation calcultes the irradiance that reaches the camera from a single surface point. The notation is kind of bad, specifically
    the L_o and L_i stuff. I guess they are separate functions but they behave the same way so its a bit confusing to follow what inputs
    they really have. The actual names are L_o is output luminance, L_i is input luminance. They just didn't want to introduce hit points in
    the integral and hid it in input luminance. Note that input luminance is basically a recursive call into the same equation so we gotta
    redo all this stuff over and over.
    
  NOTE: PBR Notes

    - Microfacet theory models a surface as a bunch of tiny perfect mirrors. Roughness gives a surface a more diffuse color while smooth
      surfaces are more specular.

      * COME BACK HERE

    - Energy Conservation is a property where we don't emit more light then we got as input. We model light as specular and diffuse. When
      light hits a surface, it gets reflected + refracted in different proportions. Reflected light is specular while refracted light
      enters the surface, gets partially absorbed, might leave the surface, and be visible as diffuse light. We normally assume diffuse
      light scatters in a very small point on the surface, unless we take SSS into account. The other special case is metallic surfaces
      which only have specular light, no diffuse.

    - Reflectance Equation is a simpler render equation. The equation is as follows:

      L_o(p, wo) = Integral(hemisphere, f_r(p, wi, wo) * L_i(p, wi) * dot(n, wi) * dwi)

      Radiant flux is energy of light over various wave lengths. The emitted energy of a source can therefore be thought of as a function
      of energy for each wave length. Wavelengths between 390nm and 700nm are visible light. If we plot it like this, radiant flux is the
      area of this function, or the integral of it. We generally approach this problem not via wave lengths but a rgb triplet for color.

      wi represents a solid angle in the equation above. This a box on a unit sphere which progressively gets smaller as we approx the
      integral better and better until it becomes a point. In reality, this could be any shape, not just a box, but the integral will
      converge to the same result.

      Radiant Intensity measures the amount of radiant flux per solid angle, so the radiant flux over a area, not from a single direction.
      Radiant Intensity = deriv of Radiant Flux / Deriv of solid angle area.

      Radiance is the total observed energy over a area A over the solid angle w of a light of radiant intensity. The area A we are looking
      at is the area of the center of the unit sphere that light is hitting, so we are essentially calculating Radiant Intensity for all
      points in a area A. The formula is L = d^2 * radiant flux / dArea * dw * cos(theta)

      For some reason, we kinda use the above to go back to radiant intensity, take a -> 0 and w -> 0 and so we can calculate the color
      of a sample as a point, not area.

      Now in the redner equation, L represents radiance of some point p coming from a direction wi, scaled by cos(theta). The equation
      calculates the reflected radiance L_o of a point p in the direction w0 (dir to the eye). Or differently, the measure of the reflected
      sum of the lights irradiance onto point p as viewed from w0. We sample irradiance along a hemisphere in the normal direction.

      We normally would do this kind of sampling to get environment lighting at a point but if we just have point lights and stuff like
      that, then we sub the integral for a sum of all lights.

    - BRDF: This is a function that takes as input incoming light dir, outgoing light dir, surf normal and a surface parameter a and
      calculates how light interacts with the microfacets/surface. To make it physically based, we need it to conserve energy. Blinn Phong
      is a BRDF but its not energy conserving. We will use the Cook Torrance BRDF instead.

      Cook Torrance = kd * f_lambert + ks * f_cooktorrance (diffuse + specular light). kd is the ratio of incoming light that gets
      refracted, ks is the ratio of light that gets reflected. flambert = c/pi with c being the surface color.
      fcook = DFG / (4*(w0 * n)(wi * n))

      D, F, G are different functions that represent normal distribution, geometry, and fresnel.

    - Normal Distribution: Approximates the amount othe surface microfacets are aligned to the halfway vector influenced by roughness.
    - Geometry Function: Describes self shadowing property of microfacets.
    - Fresnel Equation: Describes the ratio of surface reflection at different surface angles.

    
 */

#define Square(a) ((a)*(a))
const float Pi32 = 3.14159265359;

vec3 ImportanceSampleGGX(vec2 Xi, vec3 Normal, float Roughness)
{
    float A = Square(Roughness);
    float Phi = 2*Pi32*Xi.x;
    float CosTheta = sqrt((1 - Xi.y) / (1 + (Square(A) - 1) * Xi.y));
    float SinTheta = sqrt(1.0 - Square(CosTheta));

    // NOTE: Convert from spherical to cartesian coordinates
    vec3 H;
    H.x = cos(Phi) * SinTheta;
    H.y = sin(Phi) * SinTheta;
    H.z = CosTheta;

    // NOTE: Convert from tangent space to world space
    vec3 Up = abs(Normal.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 Tangent = normalize(cross(Up, Normal));
    vec3 BiTangent = cross(Normal, Tangent);
    vec3 SampleVector = Tangent * H.x + BiTangent * H.y + Normal * H.z;

    return SampleVector;
}

float NormalDistributionGGX(vec3 Normal, vec3 Halfway, float Roughness)
{
    // NOTE: This is Trowbridge-Reitz GGX
    float A = Square(Roughness);
    float ASq = Square(A);
    float NDotH = max(dot(Normal, Halfway), 0);
    float Denom = (Square(NDotH) * (ASq - 1) + 1);
    float Result = ASq / (Pi32 * Square(Denom));
    return Result;
}

float GeometrySchlickGGX(float NDotV, float Roughness)
{
    float R = Roughness + 1.0;
    float K = Square(R) / 8.0;
    float Result = NDotV / (NDotV * (1.0 - K) + K);
    return Result;
}

float GeometrySmithGGX(vec3 Normal, vec3 View, vec3 LightDir, float Roughness)
{
    // NOTE: Smiths Schlick GGX
    float NDotV = max(dot(Normal, View), 0);
    float NDotL = max(dot(Normal, LightDir), 0);
    float Result = GeometrySchlickGGX(NDotV, Roughness) * GeometrySchlickGGX(NDotL, Roughness);
    return Result;
}

vec3 FresnelSchlick(float CosTheta, vec3 SurfaceReflection)
{
    vec3 Result = SurfaceReflection + (vec3(1.0) - SurfaceReflection) * pow(1.0 - CosTheta, 5.0);
    return Result;
}

vec3 FresnelSchlickRoughness(float CosTheta, vec3 SurfaceReflection, float Roughness)
{
    vec3 Result = SurfaceReflection + (vec3(1.0 - Roughness) - SurfaceReflection) * pow(1.0 - CosTheta, 5.0);
    return Result;
}

vec3 PbrLighting(vec3 View, vec3 Halfway,
                 vec3 SurfaceColor, vec3 SurfaceNormal, vec3 SurfaceReflection, float SurfaceRoughness, float SurfaceMetallic,
                 vec3 LightDir, vec3 LightRadiance)
{
    vec3 Result = vec3(0);

    // NOTE: Cook Torrance Brdf
    vec3 Kd = vec3(0);
    vec3 Ks = vec3(0);
    vec3 Specular = vec3(0);
    {
        float D = NormalDistributionGGX(SurfaceNormal, Halfway, SurfaceRoughness);
        float G = GeometrySmithGGX(SurfaceNormal, View, LightDir, SurfaceRoughness);
        vec3 F = FresnelSchlick(max(0, dot(Halfway, View)), SurfaceReflection);

        Ks = F;
        Kd = vec3(1) - Ks;
        Kd *= 1.0 - SurfaceMetallic;

        Specular = (D*G*F) / max(4.0 * max(dot(SurfaceNormal, View), 0) * max(dot(SurfaceNormal, LightDir), 0), 0.001);
    }
    
    // NOTE: Render Equation
    Result = (Kd * SurfaceColor / Pi32 + Specular) * LightRadiance * max(dot(SurfaceNormal, LightDir), 0);
    
    return Result;
}

vec3 PbrPointLight(vec3 View,
                   vec3 SurfacePos, vec3 SurfaceColor, vec3 SurfaceNormal, vec3 SurfaceReflection, float SurfaceRoughness, float SurfaceMetallic,
                   vec3 LightPos, vec3 LightColor)
{
    vec3 LightDir = normalize(LightPos - SurfacePos);
    vec3 Halfway = normalize(View + LightDir);

    float Distance = length(LightPos - SurfacePos);
    float Attenuation = 1.0 / (Distance * Distance);
    vec3 LightRadiance = LightColor * Attenuation;

    vec3 Result = PbrLighting(View, Halfway, SurfaceColor, SurfaceNormal, SurfaceReflection, SurfaceRoughness, SurfaceMetallic, LightDir, LightRadiance);
    return Result;
}

vec3 PbrDirectionalLight(vec3 View,
                         vec3 SurfacePos, vec3 SurfaceColor, vec3 SurfaceNormal, vec3 SurfaceReflection, float SurfaceRoughness, float SurfaceMetallic,
                         vec3 LightDir, vec3 LightColor)
{
    vec3 Halfway = normalize(View + LightDir);
    vec3 Result = PbrLighting(View, Halfway, SurfaceColor, SurfaceNormal, SurfaceReflection, SurfaceRoughness, SurfaceMetallic, LightDir, LightColor);
    return Result;
}
