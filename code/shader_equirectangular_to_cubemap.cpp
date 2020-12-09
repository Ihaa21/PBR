#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_viewport_layer_array : enable

layout(set = 1, binding = 0) uniform sampler2D EquirectangularMap;

const float Pi32 = 3.14159265359;

#if FRAGMENT_SHADER

layout(location = 0) in vec3 InLocalPos;

layout(location = 0) out vec4 OutColor;

void main()
{
    /*
        NOTE: Spherical to Cartesian (inputs are x, y, z):

          r     = sqrt(x^2 + y^2 + z^2)
          theta = arctan(y/x)                 <- [-pi, pi]
          phi   = arctan(sqrt(x^2 + y^2) / z) <- [0, pi]

        Arctan is usually defined only for outputs in [-pi/2, pi/2] range. The reason is that if we consider it geometrically, we have
        opp/adj situation occuring. Once we do the division, we lose information on which was negative/positive. But those control which
        quadrant we are in. For example:

          (1, 1)   -> arctan(1/1)   = arctan(1)  = pi/4
          (-1, -1) -> arctan(-1/-1) = arctan(1)  = pi/4
          (-1, 1)  -> arctan(-1/1)  = arctan(-1) = -pi/4
          (1, -1)  -> arctan(1/-1)  = arctan(-1) = -pi/4

        All of those input vectors are in different quadrants but we lose that info after division which is where atan2 comes in.
        Interestingly, this is a situation where geometrically we preserve a more general function than algebraically. Ofcourse, we can
        account for this algebraically by converting hte input to be 2 values instead of 1 but its a bit weirder. Tan returns the ratio of
        the 2 sides, but tan is defined properly for [0, 2pi] and actually [-inf, inf]. Tan cannot return to you the ratios properly
        because of similar triangles, each angle has a infinite # of answers. So tan never needs to be modified. This is a very weird
        situation, and something I would need to think about more.

        End result is, we use atan2. Now, atan2 returns values in [-pi, pi] range but it takes the num and denom as inputs. We need to
        think of atan2 in terms of positive/negative combos in num/denom:

          pos / pos -> [0, pi/2]
          pos / neg -> [pi/2, pi]
          neg / neg -> [-pi, -pi/2]
          neg / pos -> [-pi/2, 0]

        So, back to spherical coordinates. We have the following:

          atan2(Pos.y, Pos.x)
          atan2(R, Pos.z)

        Pos.xyz all range from -inf to +inf so the first case is between -pi and pi. R however is always positive since its a distance.
        Thus, the num is positive always so we get a [0, pi] case. This is why the ranges are different.

        Looking back, its almost like we got a unique coordinate system for free. Like we couldn't have made a mistake. We had to have
        chosen R to be what it is right? Actually, probably not. If we made R the signed distance, then we would have multiple values map
        to the same point. The R being chosen as positive only was very implicit and something I completely missed but did.

        We can derive the equations a differnt way for the inverse:

          x = r * sin(phi) * cos(theta)
          y = r * sin(phi) * sin(theta)
          z = r * cos(phi)

          => phi = arccos(z / r)

          x/y = cos(theta) / sin(theta)
          y/x = tan(theta)
          arctan(y, x) = theta

        This doesn't really help us though because we still have a equation in r so the only hope is arccos is better than arctan.

        // TODO: Why doesn't acos work?
     */

    // TODO: This doesn't seem to be doing a good mapping, is it not uniform?
    // NOTE: We only keep the angles to map to our texture and we remap it to 0-1 range
    //vec2 Uv = vec2(atan(InLocalPos.y, InLocalPos.x), atan(sqrt(InLocalPos.x*InLocalPos.x + InLocalPos.y*InLocalPos.y), InLocalPos.z));
    //vec2 Uv = vec2(atan(InLocalPos.y, InLocalPos.x), acos(InLocalPos.z / sqrt(InLocalPos.x*InLocalPos.x + InLocalPos.y*InLocalPos.y)));
    //Uv.x = (Uv.x + Pi32) / (2*Pi32);
    //Uv.y = (Uv.y) / (Pi32);

    // NOTE: https://en.wikipedia.org/wiki/UV_mapping
    vec3 NormalizedPos = normalize(InLocalPos);
    vec2 Uv = vec2(0.5) + vec2(atan(NormalizedPos.x, NormalizedPos.z) * (1 / (2*Pi32)), -asin(NormalizedPos.y) / Pi32);
    
    vec3 Color = texture(EquirectangularMap, Uv).rgb;
    OutColor = vec4(Color, 1);
}

#endif
