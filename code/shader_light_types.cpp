vec3 PointLight(vec3 SurfacePos, vec3 LightPos, vec3 LightColor)
{
    vec3 Result = vec3(0);

    float Distance = length(LightPos - SurfacePos);
    float Attenuation = 1.0 / (Distance * Distance);
    Result = LightColor * Attenuation;

    return Result;
}
