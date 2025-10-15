#version 140

uniform vec4 uniform_color;
uniform float emission_factor;
uniform vec4 xyRange;   // 水平面的范围
uniform int  gradient = -1;  // 渐变方向 0  y 正方向， 1 为 y 的负方向， 2 x 的正方向， 3 y 的负方向  
uniform vec3 gradient_color;

// x = tainted, y = specular;
in vec2 intensity;
in vec2 vPos;


out vec4 frag_color;

void main()
{
    //
    float t = emission_factor;
    vec4 color;
    if(gradient == 0)
    {
      t = (vPos.y - xyRange.y) / (xyRange.w - xyRange.y);
      t = clamp(t, 0.0, 1.0);  // 保证在 0~1 范围内
      color = vec4(vec3(intensity.y) + uniform_color.rgb * (intensity.x + emission_factor), uniform_color.a);
    //vec3  rwColor = vec3(0.44,0.277,0.168);
      color.rgb = mix(color.rgb, gradient_color, t);
    }
    else if(gradient == 1)
    {
      t = (vPos.y - xyRange.y) / (xyRange.w - xyRange.y);
      t = clamp(t, 0.0, 1.0);  // 保证在 0~1 范围内
      t = 1.0 -t;
       color = vec4(vec3(intensity.y) + uniform_color.rgb * (intensity.x + emission_factor), uniform_color.a);
    //vec3  rwColor = vec3(0.44,0.277,0.168);
      color.rgb = mix(color.rgb, gradient_color, t);
    }
    else if(gradient == 2)
    {
      t = (vPos.x - xyRange.x) / (xyRange.z - xyRange.x);
      t = clamp(t, 0.0, 1.0);  // 保证在 0~1 范围内
      t = 1.0 -t;
      color = vec4(vec3(intensity.y) + uniform_color.rgb * (intensity.x + emission_factor), uniform_color.a);
    //vec3  rwColor = vec3(0.44,0.277,0.168);
      color.rgb = mix(color.rgb, gradient_color, t);
    }
    else if(gradient == 3)
    {
      t = (vPos.x - xyRange.x) / (xyRange.z - xyRange.x);
      t = clamp(t, 0.0, 1.0);  // 保证在 0~1 范围内
      t = 1.0 -t;
      color = vec4(vec3(intensity.y) + uniform_color.rgb * (intensity.x + emission_factor), uniform_color.a);
      color.rgb = mix(color.rgb, gradient_color, t);
    }
    else
    {
       color = vec4(vec3(intensity.y) + uniform_color.rgb * (intensity.x + emission_factor), uniform_color.a);
    }


    frag_color = color;
  
     
    //frag_color = vec4(vec3(intensity.y) + uniform_color.rgb * (intensity.x + emission_factor), uniform_color.a);
}
