#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_viewport_layer_array : enable

layout(set = 1, binding = 0) uniform samplerCube EnvironmentMap;

#if FRAGMENT_SHADER

layout(location = 0) in vec3 InLocalPos;

layout(location = 0) out vec4 OutColor;

const float Pi32 = 3.14159265359;

void main()
{
    /*
        NOTE: This shader makes it so that we sample a cubemap given a normal direction as a sphere, and the output is the irradiance that
              is coming into the hemisphere oriented around that normal as if there is no occlusion. Occlusion is a completely separate term
              that is lost in this process. Probably where spherical harmonics comes in.
     */

    vec3 Normal = normalize(InLocalPos);

    /*
         NOTE: Here we compute the following equation:

                 L_o(p, w_o) = k_d * c * (1/pi) * Integral(hemisphere, L_i(p, w_i) * dot(n, w_i) * dw_i)

               o stands for outgoing and i stands for incoming. We are getting how much light is outgoing from a surface with a given normal.
               Since we completely ignore the occlusion aspect of all this, the p value doesn't matter, only the normal. Normally, we would
               also care about w_o which is true for specular but diffuse is equally emissive in all directions so we can also ignore that.

               We approximate this integral via solid angles and sums. This means we use spherical coordinates on the hemisphere which
               gives us a dtheta and dphi. dphi is the rotation around the xy axis while dtheta rotates us towards z. Note that since the
               rectangles get smaller near the top of the hemisphere, the tutorial mentions we weigh them by sin(theta)dphi for horizontal
               length and dtheta for vertical. I think that this is a actual area formula but the tutorial glosses over this.

               * COME BACK TO THIS

               We get the following integral:

                 L_o(p, phi_o, theta_o) = k_d * c * (1/pi) * Integral(phi = 0, 2pi, Integral(theta = 0, pi/2, L_i(p, phi_i, theta_i) * cos(theta) * sin(theta) * dphi * dtheta))

               We then replace the integrals with sums from 0 to n1 and we replace the 1/pi with pi / (n1 * n2) since we are mapping the
               sums to discrete integers (sum steps in integers). Really you can get around this but this probably avoids float errors as
               well. We get a pi in num since we would factor out a pi^2.
    */

    vec3 Irradiance = vec3(0);
    vec3 Up = vec3(0, 1, 0);
    vec3 Right = cross(Up, Normal);
    Up = cross(Normal, Right);

    float NumSamples = 0;
    float SampleDelta = 0.025;
    for (float Phi = 0; Phi < 2*Pi32; Phi += SampleDelta)
    {
        for (float Theta = 0; Theta < Pi32*0.5; Theta += SampleDelta)
        {
            // NOTE: Spherical to cartesian conversion
            vec3 SampleVec = vec3(sin(Theta)*cos(Phi), sin(Theta)*sin(Phi), cos(Theta));
            // NOTE: Convert the above vector to the space of the normal modulated hemisphere
            vec3 WorldSampleVec = SampleVec.x * Right + SampleVec.y * Up + SampleVec.z * Normal;

            Irradiance += texture(EnvironmentMap, WorldSampleVec).rgb * cos(Theta) * sin(Theta);
            NumSamples += 1;
        }
    }

    Irradiance = Irradiance * (Pi32 / NumSamples);
    OutColor = vec4(Irradiance, 1);
}

#endif
