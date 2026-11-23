// World positions are centimetres, including when a road is a rotated bridge deck.
float2 dx = ddx(UV), dy = ddy(UV);
float determinant = max(abs(dx.x * dy.y - dx.y * dy.x), 1e-10);
float2 size = float2(length(ddx(P) * dy.y - ddy(P) * dx.y),
                     length(ddy(P) * dx.x - ddx(P) * dy.x)) / determinant;
float across = size.x < size.y ? UV.x : UV.y;
float width = min(size.x, size.y);
float laneCount = clamp(round(width / 350.), 1., 4.);
float lane = frac(across * laneCount);
float aggregate = dot(Fine, float3(.30, .59, .11));
float track = 1. - smoothstep(.035, .13, abs(abs(lane - .5) - .22));
float shoulder = 1. - smoothstep(.013, .083, min(across, 1. - across));
float dust = shoulder * lerp(.4, 1., smoothstep(.055, .21, Broad));

float3 wearingCourse = lerp(Fine, Old * float3(.64, .67, .69),
                            .24 + .24 * smoothstep(.08, .24, Broad));
wearingCourse *= .91 - track * (.075 + .04 * smoothstep(.05, .15, aggregate));
wearingCourse = lerp(wearingCourse, wearingCourse * 1.12 + float3(.049, .033, .018), dust * .65);

// Only a fraction of sites carry an old service cut. Set back each boundary from
// its cell edge, then rotate and weather the asphalt shoulders at physical scale.
float2 metres = P.xy * .01;
float2 spacing = float2(29., 12.5);
float2 cell = floor(metres / spacing);
float4 pick = frac(sin(dot(cell, float2(39.71, 17.13)) +
                       float4(.4, 5.2, 9.7, 17.6)) * 13758.545);
float2 local = (frac(metres / spacing) - .5) * spacing;
local -= (pick.xy - .5) * float2(13., 5.3);
float angle = (pick.z - .5) * .52;
float co = cos(angle), si = sin(angle);
float2 rotated = float2(co * local.x + si * local.y, -si * local.x + co * local.y);
float2 extent = lerp(float2(1.05, .36), float2(3.85, 1.08), pick.zy);
float radius = .11 + .22 * pick.x;
float2 corner = abs(rotated) - extent + radius;
float edge = length(max(corner, 0.)) + min(max(corner.x, corner.y), 0.) - radius;
edge += sin(metres.y * 3.5 + metres.x * 1.8) * .044 +
        sin(metres.x * 7.1 - metres.y * 3.8) * .024 + (Broad - .17) * .22;
float filterWidth = max(.016, fwidth(edge));
float presence = smoothstep(.56, .72, pick.w);
float infill = (1. - smoothstep(-.04 - filterWidth, .07 + filterWidth, edge)) * presence;
float tar = (1. - smoothstep(.012, .039 + filterWidth, abs(edge))) * presence;
float3 replacement = lerp(wearingCourse, Old * float3(.64, .62, .58), .32);
replacement *= .88 + .18 * pick.y;
float3 surface = lerp(wearingCourse, replacement, infill);
return lerp(surface, surface * .65, tar * .62);
