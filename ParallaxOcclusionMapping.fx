//--------------------------------------------------------------------------------------
// File: ParallaxOcclusionMapping.fx
//
// 視差遮蔽マッピング（Parallax Occlusion Mapping, POM）の実装
//
// 次の論文で説明されているアルゴリズムの実装：
// "Dynamic Parallax Occlusion Mapping with Approximate Soft Shadows"
// 著者: N. Tatarchuk（ATI Research）
// ACM Symposium on Interactive 3D Graphics and Games, 2006 掲載予定
//
// 実時間シーンでの使用例は ATI X1K デモ "ToyShop" を参照:
//    http://www.ati.com/developer/demos/rx1800.html
//
// Copyright (c) ATI Research, Inc. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// グローバル変数
//--------------------------------------------------------------------------------------

texture g_baseTexture;              // ベースカラー（アルベド）テクスチャ
texture g_nmhTexture;               // 法線マップ＋高さマップの複合テクスチャ

float4 g_materialAmbientColor;      // マテリアルの環境色
float4 g_materialDiffuseColor;      // マテリアルの拡散反射色
float4 g_materialSpecularColor;     // マテリアルの鏡面反射色

float  g_fSpecularExponent;         // 鏡面ハイライトの指数
bool   g_bAddSpecular;              // 鏡面成分を有効化するかどうか

// 光源パラメータ:
float3 g_LightDir;                  // 光の方向（ワールド空間）
float4 g_LightDiffuse;              // 光の拡散色
float4 g_LightAmbient;              // 光の環境色

float4   g_vEye;                    // カメラ位置
float    g_fBaseTextureRepeat;      // ベース／法線テクスチャのタイリング係数
float    g_fHeightMapScale;         // 高さマップの有効な値域（スケール）を表す

// 行列:
float4x4 g_mWorld;                  // オブジェクトのワールド行列
float4x4 g_mWorldViewProjection;    // World * View * Projection 行列
float4x4 g_mView;                   // ビュー行列

bool     g_bVisualizeLOD;           // LOD の可視化（色分け）を行うか
bool     g_bVisualizeMipLevel;      // ミップレベルの可視化を行うか
bool     g_bDisplayShadows;         // セルフオクルージョンに基づく影を表示するか

float2   g_vTextureDims = float2(512.f, 512.f); // 実行時にミップレベルを計算するための
                                    // テクスチャ寸法（幅, 高さ）を指定

int      g_nLODThreshold;           // 視差遮蔽マッピング（フル計算）から
                                    // バンプマッピングへ切り替えるミップレベルしきい値

float    g_fShadowSoftening;        // ソフトシャドウのぼかし係数

int      g_nMinSamples;             // 高さプロファイルをサンプリングする最小サンプル数
int      g_nMaxSamples;             // 高さプロファイルをサンプリングする最大サンプル数



//--------------------------------------------------------------------------------------
// テクスチャサンプラ
//--------------------------------------------------------------------------------------
sampler tBase =
sampler_state
{
    Texture = < g_baseTexture >;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};
sampler tNormalHeightMap =
sampler_state
{
    Texture = < g_nmhTexture >;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};


//--------------------------------------------------------------------------------------
// 頂点シェーダ出力構造体
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 position          : POSITION;
    float2 texCoord          : TEXCOORD0;
    float3 vLightTS          : TEXCOORD1;   // 接空間の光ベクトル（正規化していない）
    float3 vViewTS           : TEXCOORD2;   // 接空間の視線ベクトル（正規化していない）
    float2 vParallaxOffsetTS : TEXCOORD3;   // 接空間での視差オフセットベクトル
    float3 vNormalWS         : TEXCOORD4;   // ワールド空間の法線
    float3 vViewWS           : TEXCOORD5;   // ワールド空間の視線ベクトル
};


//--------------------------------------------------------------------------------------
// 標準的な変換とライティングに必要な情報を計算
//--------------------------------------------------------------------------------------
VS_OUTPUT RenderSceneVS( float4 inPositionOS  : POSITION,
                         float2 inTexCoord    : TEXCOORD0,
                         float3 vInNormalOS   : NORMAL,
                         float3 vInBinormalOS : BINORMAL,
                         float3 vInTangentOS  : TANGENT )
{
    VS_OUTPUT Out;

    // 位置を変換して出力
    Out.position = mul( inPositionOS, g_mWorldViewProjection );

    // テクスチャ座標を伝播（タイル係数を適用）
    Out.texCoord = inTexCoord * g_fBaseTextureRepeat;

    // 法線・Tangent・Binormal をオブジェクト空間→ワールド空間へ変換
    float3 vNormalWS   = mul( vInNormalOS,   (float3x3) g_mWorld );
    float3 vTangentWS  = mul( vInTangentOS,  (float3x3) g_mWorld );
    float3 vBinormalWS = mul( vInBinormalOS, (float3x3) g_mWorld );

    // ワールド空間の頂点法線をそのまま出力
    Out.vNormalWS = vNormalWS;

    vNormalWS   = normalize( vNormalWS );
    vTangentWS  = normalize( vTangentWS );
    vBinormalWS = normalize( vBinormalWS );

    // ワールド空間での位置
    float4 vPositionWS = mul( inPositionOS, g_mWorld );

    // ワールド空間の視線ベクトル（正規化せず）を計算して出力
    float3 vViewWS = g_vEye - vPositionWS;
    Out.vViewWS = vViewWS;

    // ワールド空間の光ベクトル（正規化せず）
    float3 vLightWS = g_LightDir;

    // 光・視線ベクトルを正規化し、接空間へ変換
    float3x3 mWorldToTangent = float3x3( vTangentWS, vBinormalWS, vNormalWS );

    // 接空間の光・視線ベクトルを出力
    Out.vLightTS = mul( vLightWS, mWorldToTangent );
    Out.vViewTS  = mul( mWorldToTangent, vViewWS  );

    // 高さプロファイルと視線レイの交差計算に用いるレイ方向を求める
    // （導出は上記論文を参照）

    // 視差の初期オフセット方向（接空間の xy を正規化）
    float2 vParallaxDirection = normalize(  Out.vViewTS.xy );

    // このベクトルの長さが最大変位量を決める
    float fLength         = length( Out.vViewTS );
    float fParallaxLength = sqrt( fLength * fLength - Out.vViewTS.z * Out.vViewTS.z ) / Out.vViewTS.z;

    // 実際の「逆向き」視差オフセットベクトルを計算
    Out.vParallaxOffsetTS = vParallaxDirection * fParallaxLength;

    // 高さマップの値域の違いを補正するため、アーティスト調整用のスケールを適用
    Out.vParallaxOffsetTS *= g_fHeightMapScale;

   return Out;
}


//--------------------------------------------------------------------------------------
// ピクセルシェーダ出力構造体
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 RGBColor : COLOR0;  // 出力色
};

struct PS_INPUT
{
   float2 texCoord          : TEXCOORD0;
   float3 vLightTS          : TEXCOORD1;   // 接空間の光ベクトル（正規化していない）
   float3 vViewTS           : TEXCOORD2;   // 接空間の視線ベクトル（正規化していない）
   float2 vParallaxOffsetTS : TEXCOORD3;   // 接空間での視差オフセット
   float3 vNormalWS         : TEXCOORD4;   // ワールド空間の法線
   float3 vViewWS           : TEXCOORD5;   // ワールド空間の視線ベクトル
};


//--------------------------------------------------------------------------------------
// 関数:    ComputeIllumination
//
// 説明:    指定ピクセルの属性テクスチャと光ベクトルを用いて
//          Phong 風の照明を計算する
//--------------------------------------------------------------------------------------
float4 ComputeIllumination( float2 texCoord, float3 vLightTS, float3 vViewTS, float fOcclusionShadow )
{
   // 法線マップから法線（接空間）をサンプルして正規化
   float3 vNormalTS = normalize( tex2D( tNormalHeightMap, texCoord ) * 2 - 1 );

   // ベースカラーをサンプル
   float4 cBaseColor = tex2D( tBase, texCoord );

   // 拡散反射成分を計算
   float3 vLightTSAdj = float3( vLightTS.x, -vLightTS.y, vLightTS.z );

   float4 cDiffuse = saturate( dot( vNormalTS, vLightTSAdj )) * g_materialDiffuseColor;

   // 必要であれば鏡面成分を計算
   float4 cSpecular = 0;
   if ( g_bAddSpecular )
   {
      float3 vReflectionTS = normalize( 2 * dot( vViewTS, vNormalTS ) * vNormalTS - vViewTS );

      float fRdotL = saturate( dot( vReflectionTS, vLightTSAdj ));
      cSpecular = saturate( pow( fRdotL, g_fSpecularExponent )) * g_materialSpecularColor;
   }

   // 最終色を合成
   float4 cFinalColor = (( g_materialAmbientColor + cDiffuse ) * cBaseColor + cSpecular ) * fOcclusionShadow;

   return cFinalColor;
}


//--------------------------------------------------------------------------------------
// 視差遮蔽マッピング（POM）のピクセルシェーダ
//
// 注意: このシェーダには教育目的のモードが含まれています。
//       実際のゲームや複雑なシーンでは、視覚品質（LOD、スペキュラ、影など）を
//       トグルする処理は別の最適な方法で行うべきです。
//       ここでは教材として分かりやすく実装しています。
//--------------------------------------------------------------------------------------
float4 RenderScenePS( PS_INPUT i ) : COLOR0
{

   // 補間されたベクトルを正規化
   float3 vViewTS   = normalize( i.vViewTS  );
   float3 vViewWS   = normalize( i.vViewWS  );
   float3 vLightTS  = normalize( i.vLightTS );
   float3 vNormalWS = normalize( i.vNormalWS );

   float4 cResultColor = float4( 0, 0, 0, 1 );

   // ピクセルシェーダ内でミップレベルを明示的に計算し、
   // フル POM からバンプマッピングへの LOD 切替を行う適応式の実装。
   // 詳細と利点は前述の論文を参照。

   // 現在の勾配を計算
   float2 fTexCoordsPerSize = i.texCoord * g_vTextureDims;

   // ddx/ddy で x, y の微分を一度に取得し最適化
   float2 dxSize, dySize;
   float2 dx, dy;

   float4( dxSize, dx ) = ddx( float4( fTexCoordsPerSize, i.texCoord ) );
   float4( dySize, dy ) = ddy( float4( fTexCoordsPerSize, i.texCoord ) );

   float  fMipLevel;
   float  fMipLevelInt;    // ミップレベルの整数部
   float  fMipLevelFrac;   // ミップレベルの小数部（補間に使用）

   float  fMinTexCoordDelta;
   float2 dTexCoords;

   // u と v の変化量の二乗和（マイナムーブ）
   dTexCoords = dxSize * dxSize + dySize * dySize;

   // 標準的なミップマッピングでは max を用いる
   fMinTexCoordDelta = max( dTexCoords.x, dTexCoords.y );

   // 現在のミップレベルを計算（*0.5 は実質的に sqrt に相当）
   fMipLevel = max( 0.5 * log2( fMinTexCoordDelta ), 0 );

   // まずは入力のテクスチャ座標でサンプル（=バンプマップ相当）
   float2 texSample = i.texCoord;

   // LOD 視覚化のための乗算色（g_nLODThreshold の説明参照）
   float4 cLODColoring = float4( 1, 1, 3, 1 );

   float fOcclusionShadow = 1.0;

   if ( fMipLevel <= (float) g_nLODThreshold )
   {
      //===============================================//
      // 視差遮蔽マッピングのオフセット計算          //
      //===============================================//

      // 動的フロー制御により、視角に応じてサンプル数を変更。
      // 斜め視ほどステップを細かくして精度を上げる。
      // 幾何法線と視線の角度に対し線形にサンプル密度を変える。
      int nNumSteps = (int) lerp( g_nMaxSamples, g_nMinSamples, dot( vViewWS, vNormalWS ) );

      // 視差オフセット方向（頂点シェーダで計算）に沿って、
      // 高さプロファイルと視線レイの交差を見つける。
      // このコードは HLSL の動的フロー制御に特化しており、構文に敏感。
      // 他の例へ移植しても動的分岐を維持したい場合は注意が必要。
      //
      // 以下では高さプロファイルを区分線形で近似する。
      // レイとプロファイルの交点を含む2点を見つけ、
      // その2点で張る線分とレイの交差を計算してオフセットを得る。
      // 詳細は前述の論文を参照。
      //

      float fCurrHeight = 0.0;
      float fStepSize   = 1.0 / (float) nNumSteps;
      float fPrevHeight = 1.0;

      int    nStepIndex = 0;

      float2 vTexOffsetPerStep = fStepSize * i.vParallaxOffsetTS;
      float2 vTexCurrentOffset = i.texCoord;
      float  fCurrentBound     = 1.0;
      float  fParallaxAmount   = 0.0;

      float2 pt1 = 0;
      float2 pt2 = 0;

      float2 texOffset2 = 0;

      while ( nStepIndex < nNumSteps )
      {
         vTexCurrentOffset -= vTexOffsetPerStep;

         // 高さマップをサンプル（本例では法線マップのアルファに格納）
         fCurrHeight = tex2Dgrad( tNormalHeightMap, vTexCurrentOffset, dx, dy ).a;

         fCurrentBound -= fStepSize;

         if ( fCurrHeight > fCurrentBound )
         {
            pt1 = float2( fCurrentBound, fCurrHeight );
            pt2 = float2( fCurrentBound + fStepSize, fPrevHeight );

            texOffset2 = vTexCurrentOffset - vTexOffsetPerStep;

            nStepIndex = nNumSteps + 1;
            fPrevHeight = fCurrHeight;
         }
         else
         {
            nStepIndex++;
            fPrevHeight = fCurrHeight;
         }
      }

      float fDelta2 = pt2.x - pt2.y;
      float fDelta1 = pt1.x - pt1.y;

      float fDenominator = fDelta2 - fDelta1;

      // SM3.0 では 0 除算は Inf を生成してしまうため、チェックが必要
      if ( fDenominator == 0.0f )
      {
         fParallaxAmount = 0.0f;
      }
      else
      {
         fParallaxAmount = (pt1.x * fDelta2 - pt2.x * fDelta1 ) / fDenominator;
      }

      float2 vParallaxOffset = i.vParallaxOffsetTS * (1 - fParallaxAmount );

      // 疑似的に押し出された表面上の最終テクスチャ座標
      float2 texSampleBase = i.texCoord - vParallaxOffset;
      texSample = texSampleBase;

      // 閾値付近（遷移領域）の場合のみバンプマップへ補間
      cLODColoring = float4( 1, 1, 1, 1 );

      if ( fMipLevel > (float)(g_nLODThreshold - 1) )
      {
         // 小数部で補間割合を決定
         fMipLevelFrac = modf( fMipLevel, fMipLevelInt );

         if ( g_bVisualizeLOD )
         {
            // 可視化用：POM結果→青色（遷移層）へ補間表示
            cLODColoring = float4( 1, 1, max( 1, 2 * fMipLevelFrac ), 1 );
         }

         // POM 座標からバンプマップ座標へ、現在のミップレベルに応じて滑らかに補間
         texSample = lerp( texSampleBase, i.texCoord, fMipLevelFrac );
     }

     if ( g_bDisplayShadows == true )
     {
        float2 vLightRayTS = vLightTS.xy * g_fHeightMapScale;

        // 高さプロファイルのセルフオクルージョンを考慮した
        // ソフトでぼけた影を計算
        float sh0 =  tex2Dgrad( tNormalHeightMap, texSampleBase, dx, dy ).a;
        float shA = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.88, dx, dy ).a - sh0 - 0.88 ) *  1 * g_fShadowSoftening;
        float sh9 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.77, dx, dy ).a - sh0 - 0.77 ) *  2 * g_fShadowSoftening;
        float sh8 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.66, dx, dy ).a - sh0 - 0.66 ) *  4 * g_fShadowSoftening;
        float sh7 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.55, dx, dy ).a - sh0 - 0.55 ) *  6 * g_fShadowSoftening;
        float sh6 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.44, dx, dy ).a - sh0 - 0.44 ) *  8 * g_fShadowSoftening;
        float sh5 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.33, dx, dy ).a - sh0 - 0.33 ) * 10 * g_fShadowSoftening;
        float sh4 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.22, dx, dy ).a - sh0 - 0.22 ) * 12 * g_fShadowSoftening;

        // 実際の影強度を計算
        fOcclusionShadow = 1 - max( max( max( max( max( max( shA, sh9 ), sh8 ), sh7 ), sh6 ), sh5 ), sh4 );

        // 上記計算はやや明るくなりがちなので補正
        fOcclusionShadow = fOcclusionShadow * 0.6 + 0.4;
     }
   }

   // ピクセルの最終色を計算
   // ライトをちゃんとやるか否か
   if (false)
   {
      cResultColor = ComputeIllumination(texSample, vLightTS, vViewTS, fOcclusionShadow);
   }
   else
   {
      cResultColor = tex2D( tBase, texSample );
   }

   if ( g_bVisualizeLOD )
   {
      cResultColor *= cLODColoring;
   }

   // ミップレベルの可視化：閾値外の領域では青系に着色
   if ( g_bVisualizeMipLevel )
   {
      cResultColor = fMipLevel.xxxx;
   }

   // HDR を使う場合は出力前にトーンマップが必要。
   // この例では行っていないため、計算した色をそのまま出力する。
   return cResultColor;
}


//--------------------------------------------------------------------------------------
// バンプマッピング用シェーダ
//--------------------------------------------------------------------------------------
float4 RenderSceneBumpMapPS( PS_INPUT i ) : COLOR0
{
   // 補間ベクトルの正規化
   float3 vViewTS   = normalize( i.vViewTS  );
   float3 vLightTS  = normalize( i.vLightTS );

   float4 cResultColor = float4( 0, 0, 0, 1 );

   // 入力 UV でサンプル（バンプマップ相当）
   float2 texSample = i.texCoord;

   // 最終色を計算
   cResultColor = ComputeIllumination( texSample, vLightTS, vViewTS, 1.0f );

   // HDR の場合は出力前にトーンマップが必要。
   // 本例ではそのまま出力。
   return cResultColor;
}


//--------------------------------------------------------------------------------------
// オフセット・リミット方式によるパララックスマッピングを適用
//--------------------------------------------------------------------------------------
float4 RenderSceneParallaxMappingPS( PS_INPUT i ) : COLOR0
{
   const float sfHeightBias = 0.01;

   // 補間ベクトルの正規化
   float3 vViewTS   = normalize( i.vViewTS  );
   float3 vLightTS  = normalize( i.vLightTS );

   // 現在のテクスチャ座標で高さマップをサンプル
   float fCurrentHeight = tex2D( tNormalHeightMap, i.texCoord ).a;

   // スケールとバイアスを適用
   float fHeight = fCurrentHeight * g_fHeightMapScale + sfHeightBias;

   // 必要ならオフセット・リミットを実施
   fHeight /= vViewTS.z;

   // 視差近似のオフセットベクトルを計算
   float2 texSample = i.texCoord + vViewTS.xy * fHeight;

   float4 cResultColor = float4( 0, 0, 0, 1 );

   // 最終色を計算
   cResultColor = ComputeIllumination( texSample, vLightTS, vViewTS, 1.0f );

   // HDR の場合は出力前にトーンマップが必要。
   // 本例ではそのまま出力。
   return cResultColor;
}


//--------------------------------------------------------------------------------------
// シーンをレンダーターゲットへ描画
//--------------------------------------------------------------------------------------
technique RenderSceneWithPOM
{
    pass P0
    {
        VertexShader = compile vs_3_0 RenderSceneVS();
        PixelShader  = compile ps_3_0 RenderScenePS();
    }
}

technique RenderSceneWithBumpMap
{
    pass P0
    {
        VertexShader = compile vs_2_0 RenderSceneVS();
        PixelShader  = compile ps_2_0 RenderSceneBumpMapPS();
    }
}
technique RenderSceneWithPM
{
    pass P0
    {
        VertexShader = compile vs_2_0 RenderSceneVS();
        PixelShader  = compile ps_2_0 RenderSceneParallaxMappingPS();
    }
}

#if 0

//--------------------------------------------------------------------------------------
// File: ParallaxOcclusionMapping.fx
//
// Parallax occlusion mapping implementation
//                                        
// Implementation of the algorithm as described in "Dynamic Parallax Occlusion
// Mapping with Approximate Soft Shadows" paper, by N. Tatarchuk, ATI Research, 
// to appear in the proceedings of ACM Symposium on Interactive 3D Graphics and Games, 2006.                                            
//                                                                               
// For examples of use in a real-time scene, see ATI X1K demo "ToyShop":         
//    http://www.ati.com/developer/demos/rx1800.html                             
//
// Copyright (c) ATI Research, Inc. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

texture g_baseTexture;              // Base color texture
texture g_nmhTexture;               // Normal map and height map texture pair

float4 g_materialAmbientColor;      // Material's ambient color
float4 g_materialDiffuseColor;      // Material's diffuse color
float4 g_materialSpecularColor;     // Material's specular color

float  g_fSpecularExponent;         // Material's specular exponent
bool   g_bAddSpecular;              // Toggles rendering with specular or without

// Light parameters:
float3 g_LightDir;                  // Light's direction in world space
float4 g_LightDiffuse;              // Light's diffuse color
float4 g_LightAmbient;              // Light's ambient color

float4   g_vEye;                    // Camera's location
float    g_fBaseTextureRepeat;      // The tiling factor for base and normal map textures
float    g_fHeightMapScale;         // Describes the useful range of values for the height field

// Matrices:
float4x4 g_mWorld;                  // World matrix for object
float4x4 g_mWorldViewProjection;    // World * View * Projection matrix
float4x4 g_mView;                   // View matrix 

bool     g_bVisualizeLOD;           // Toggles visualization of level of detail colors
bool     g_bVisualizeMipLevel;      // Toggles visualization of mip level
bool     g_bDisplayShadows;         // Toggles display of self-occlusion based shadows

float2   g_vTextureDims;            // Specifies texture dimensions for computation of mip level at 
                                    // render time (width, height)

int      g_nLODThreshold;           // The mip level id for transitioning between the full computation
                                    // for parallax occlusion mapping and the bump mapping computation

float    g_fShadowSoftening;        // Blurring factor for the soft shadows computation

int      g_nMinSamples;             // The minimum number of samples for sampling the height field profile
int      g_nMaxSamples;             // The maximum number of samples for sampling the height field profile



//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler tBase = 
sampler_state
{
    Texture = < g_baseTexture >;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};
sampler tNormalHeightMap = 
sampler_state
{
    Texture = < g_nmhTexture >;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};


//--------------------------------------------------------------------------------------
// Vertex shader output structure
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 position          : POSITION;
    float2 texCoord          : TEXCOORD0;
    float3 vLightTS          : TEXCOORD1;   // light vector in tangent space, denormalized
    float3 vViewTS           : TEXCOORD2;   // view vector in tangent space, denormalized
    float2 vParallaxOffsetTS : TEXCOORD3;   // Parallax offset vector in tangent space
    float3 vNormalWS         : TEXCOORD4;   // Normal vector in world space
    float3 vViewWS           : TEXCOORD5;   // View vector in world space
    
};  


//--------------------------------------------------------------------------------------
// This shader computes standard transform and lighting
//--------------------------------------------------------------------------------------
VS_OUTPUT RenderSceneVS( float4 inPositionOS  : POSITION, 
                         float2 inTexCoord    : TEXCOORD0,
                         float3 vInNormalOS   : NORMAL,
                         float3 vInBinormalOS : BINORMAL,
                         float3 vInTangentOS  : TANGENT )
{
    VS_OUTPUT Out;
        
    // Transform and output input position 
    Out.position = mul( inPositionOS, g_mWorldViewProjection );
       
    // Propagate texture coordinate through:
    Out.texCoord = inTexCoord * g_fBaseTextureRepeat;

    // Transform the normal, tangent and binormal vectors from object space to homogeneous projection space:
    float3 vNormalWS   = mul( vInNormalOS,   (float3x3) g_mWorld );
    float3 vTangentWS  = mul( vInTangentOS,  (float3x3) g_mWorld );
    float3 vBinormalWS = mul( vInBinormalOS, (float3x3) g_mWorld );
    
    // Propagate the world space vertex normal through:   
    Out.vNormalWS = vNormalWS;
    
    vNormalWS   = normalize( vNormalWS );
    vTangentWS  = normalize( vTangentWS );
    vBinormalWS = normalize( vBinormalWS );
    
    // Compute position in world space:
    float4 vPositionWS = mul( inPositionOS, g_mWorld );
                 
    // Compute and output the world view vector (unnormalized):
    float3 vViewWS = g_vEye - vPositionWS;
    Out.vViewWS = vViewWS;

    // Compute denormalized light vector in world space:
    float3 vLightWS = g_LightDir;
       
    // Normalize the light and view vectors and transform it to the tangent space:
    float3x3 mWorldToTangent = float3x3( vTangentWS, vBinormalWS, vNormalWS );
       
    // Propagate the view and the light vectors (in tangent space):
    Out.vLightTS = mul( vLightWS, mWorldToTangent );
    Out.vViewTS  = mul( mWorldToTangent, vViewWS  );
       
    // Compute the ray direction for intersecting the height field profile with 
    // current view ray. See the above paper for derivation of this computation.
         
    // Compute initial parallax displacement direction:
    float2 vParallaxDirection = normalize(  Out.vViewTS.xy );
       
    // The length of this vector determines the furthest amount of displacement:
    float fLength         = length( Out.vViewTS );
    float fParallaxLength = sqrt( fLength * fLength - Out.vViewTS.z * Out.vViewTS.z ) / Out.vViewTS.z; 
       
    // Compute the actual reverse parallax displacement vector:
    Out.vParallaxOffsetTS = vParallaxDirection * fParallaxLength;
       
    // Need to scale the amount of displacement to account for different height ranges
    // in height maps. This is controlled by an artist-editable parameter:
    Out.vParallaxOffsetTS *= g_fHeightMapScale;

   return Out;
}   


//--------------------------------------------------------------------------------------
// Pixel shader output structure
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 RGBColor : COLOR0;  // Pixel color    
};

struct PS_INPUT
{
   float2 texCoord          : TEXCOORD0;
   float3 vLightTS          : TEXCOORD1;   // light vector in tangent space, denormalized
   float3 vViewTS           : TEXCOORD2;   // view vector in tangent space, denormalized
   float2 vParallaxOffsetTS : TEXCOORD3;   // Parallax offset vector in tangent space
   float3 vNormalWS         : TEXCOORD4;   // Normal vector in world space
   float3 vViewWS           : TEXCOORD5;   // View vector in world space
};


//--------------------------------------------------------------------------------------
// Function:    ComputeIllumination
// 
// Description: Computes phong illumination for the given pixel using its attribute 
//              textures and a light vector.
//--------------------------------------------------------------------------------------
float4 ComputeIllumination( float2 texCoord, float3 vLightTS, float3 vViewTS, float fOcclusionShadow )
{
   // Sample the normal from the normal map for the given texture sample:
   float3 vNormalTS = normalize( tex2D( tNormalHeightMap, texCoord ) * 2 - 1 );
   
   // Sample base map:
   float4 cBaseColor = tex2D( tBase, texCoord );
   
   // Compute diffuse color component:
   float3 vLightTSAdj = float3( vLightTS.x, -vLightTS.y, vLightTS.z );
   
   float4 cDiffuse = saturate( dot( vNormalTS, vLightTSAdj )) * g_materialDiffuseColor;
   
   // Compute the specular component if desired:  
   float4 cSpecular = 0;
   if ( g_bAddSpecular )
   {
      float3 vReflectionTS = normalize( 2 * dot( vViewTS, vNormalTS ) * vNormalTS - vViewTS );
           
      float fRdotL = saturate( dot( vReflectionTS, vLightTSAdj ));
      cSpecular = saturate( pow( fRdotL, g_fSpecularExponent )) * g_materialSpecularColor;
   }
   
   // Composite the final color:
   float4 cFinalColor = (( g_materialAmbientColor + cDiffuse ) * cBaseColor + cSpecular ) * fOcclusionShadow; 
   
   return cFinalColor;  
}   
 

//--------------------------------------------------------------------------------------
// Parallax occlusion mapping pixel shader
//
// Note: this shader contains several educational modes that would not be in the final
//       game or other complicated scene rendering. The blocks of code in various "if"
//       statements for turning off visual qualities (such as visual level of detail
//       or specular or shadows, etc), can be handled differently, and more optimally.
//       It is implemented here purely for educational purposes.
//--------------------------------------------------------------------------------------
float4 RenderScenePS( PS_INPUT i ) : COLOR0
{ 

   //  Normalize the interpolated vectors:
   float3 vViewTS   = normalize( i.vViewTS  );
   float3 vViewWS   = normalize( i.vViewWS  );
   float3 vLightTS  = normalize( i.vLightTS );
   float3 vNormalWS = normalize( i.vNormalWS );
     
   float4 cResultColor = float4( 0, 0, 0, 1 );

   // Adaptive in-shader level-of-detail system implementation. Compute the 
   // current mip level explicitly in the pixel shader and use this information 
   // to transition between different levels of detail from the full effect to 
   // simple bump mapping. See the above paper for more discussion of the approach
   // and its benefits.
   
   // Compute the current gradients:
   float2 fTexCoordsPerSize = i.texCoord * g_vTextureDims;

   // Compute all 4 derivatives in x and y in a single instruction to optimize:
   float2 dxSize, dySize;
   float2 dx, dy;

   float4( dxSize, dx ) = ddx( float4( fTexCoordsPerSize, i.texCoord ) );
   float4( dySize, dy ) = ddy( float4( fTexCoordsPerSize, i.texCoord ) );
                  
   float  fMipLevel;      
   float  fMipLevelInt;    // mip level integer portion
   float  fMipLevelFrac;   // mip level fractional amount for blending in between levels

   float  fMinTexCoordDelta;
   float2 dTexCoords;

   // Find min of change in u and v across quad: compute du and dv magnitude across quad
   dTexCoords = dxSize * dxSize + dySize * dySize;

   // Standard mipmapping uses max here
   fMinTexCoordDelta = max( dTexCoords.x, dTexCoords.y );

   // Compute the current mip level  (* 0.5 is effectively computing a square root before )
   fMipLevel = max( 0.5 * log2( fMinTexCoordDelta ), 0 );
    
   // Start the current sample located at the input texture coordinate, which would correspond
   // to computing a bump mapping result:
   float2 texSample = i.texCoord;
   
   // Multiplier for visualizing the level of detail (see notes for 'nLODThreshold' variable
   // for how that is done visually)
   float4 cLODColoring = float4( 1, 1, 3, 1 );

   float fOcclusionShadow = 1.0;

   if ( fMipLevel <= (float) g_nLODThreshold )
   {
      //===============================================//
      // Parallax occlusion mapping offset computation //
      //===============================================//

      // Utilize dynamic flow control to change the number of samples per ray 
      // depending on the viewing angle for the surface. Oblique angles require 
      // smaller step sizes to achieve more accurate precision for computing displacement.
      // We express the sampling rate as a linear function of the angle between 
      // the geometric normal and the view direction ray:
      int nNumSteps = (int) lerp( g_nMaxSamples, g_nMinSamples, dot( vViewWS, vNormalWS ) );

      // Intersect the view ray with the height field profile along the direction of
      // the parallax offset ray (computed in the vertex shader. Note that the code is
      // designed specifically to take advantage of the dynamic flow control constructs
      // in HLSL and is very sensitive to specific syntax. When converting to other examples,
      // if still want to use dynamic flow control in the resulting assembly shader,
      // care must be applied.
      // 
      // In the below steps we approximate the height field profile as piecewise linear
      // curve. We find the pair of endpoints between which the intersection between the 
      // height field profile and the view ray is found and then compute line segment
      // intersection for the view ray and the line segment formed by the two endpoints.
      // This intersection is the displacement offset from the original texture coordinate.
      // See the above paper for more details about the process and derivation.
      //

      float fCurrHeight = 0.0;
      float fStepSize   = 1.0 / (float) nNumSteps;
      float fPrevHeight = 1.0;
      float fNextHeight = 0.0;

      int    nStepIndex = 0;
      bool   bCondition = true;

      float2 vTexOffsetPerStep = fStepSize * i.vParallaxOffsetTS;
      float2 vTexCurrentOffset = i.texCoord;
      float  fCurrentBound     = 1.0;
      float  fParallaxAmount   = 0.0;

      float2 pt1 = 0;
      float2 pt2 = 0;
       
      float2 texOffset2 = 0;

      while ( nStepIndex < nNumSteps ) 
      {
         vTexCurrentOffset -= vTexOffsetPerStep;

         // Sample height map which in this case is stored in the alpha channel of the normal map:
         fCurrHeight = tex2Dgrad( tNormalHeightMap, vTexCurrentOffset, dx, dy ).a;

         fCurrentBound -= fStepSize;

         if ( fCurrHeight > fCurrentBound ) 
         {   
            pt1 = float2( fCurrentBound, fCurrHeight );
            pt2 = float2( fCurrentBound + fStepSize, fPrevHeight );

            texOffset2 = vTexCurrentOffset - vTexOffsetPerStep;

            nStepIndex = nNumSteps + 1;
            fPrevHeight = fCurrHeight;
         }
         else
         {
            nStepIndex++;
            fPrevHeight = fCurrHeight;
         }
      }   

      float fDelta2 = pt2.x - pt2.y;
      float fDelta1 = pt1.x - pt1.y;
      
      float fDenominator = fDelta2 - fDelta1;
      
      // SM 3.0 requires a check for divide by zero, since that operation will generate
      // an 'Inf' number instead of 0, as previous models (conveniently) did:
      if ( fDenominator == 0.0f )
      {
         fParallaxAmount = 0.0f;
      }
      else
      {
         fParallaxAmount = (pt1.x * fDelta2 - pt2.x * fDelta1 ) / fDenominator;
      }
      
      float2 vParallaxOffset = i.vParallaxOffsetTS * (1 - fParallaxAmount );

      // The computed texture offset for the displaced point on the pseudo-extruded surface:
      float2 texSampleBase = i.texCoord - vParallaxOffset;
      texSample = texSampleBase;

      // Lerp to bump mapping only if we are in between, transition section:
        
      cLODColoring = float4( 1, 1, 1, 1 ); 

      if ( fMipLevel > (float)(g_nLODThreshold - 1) )
      {
         // Lerp based on the fractional part:
         fMipLevelFrac = modf( fMipLevel, fMipLevelInt );

         if ( g_bVisualizeLOD )
         {
            // For visualizing: lerping from regular POM-resulted color through blue color for transition layer:
            cLODColoring = float4( 1, 1, max( 1, 2 * fMipLevelFrac ), 1 ); 
         }

         // Lerp the texture coordinate from parallax occlusion mapped coordinate to bump mapping
         // smoothly based on the current mip level:
         texSample = lerp( texSampleBase, i.texCoord, fMipLevelFrac );

     }  
      
     if ( g_bDisplayShadows == true )
     {
        float2 vLightRayTS = vLightTS.xy * g_fHeightMapScale;
      
        // Compute the soft blurry shadows taking into account self-occlusion for 
        // features of the height field:
   
        float sh0 =  tex2Dgrad( tNormalHeightMap, texSampleBase, dx, dy ).a;
        float shA = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.88, dx, dy ).a - sh0 - 0.88 ) *  1 * g_fShadowSoftening;
        float sh9 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.77, dx, dy ).a - sh0 - 0.77 ) *  2 * g_fShadowSoftening;
        float sh8 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.66, dx, dy ).a - sh0 - 0.66 ) *  4 * g_fShadowSoftening;
        float sh7 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.55, dx, dy ).a - sh0 - 0.55 ) *  6 * g_fShadowSoftening;
        float sh6 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.44, dx, dy ).a - sh0 - 0.44 ) *  8 * g_fShadowSoftening;
        float sh5 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.33, dx, dy ).a - sh0 - 0.33 ) * 10 * g_fShadowSoftening;
        float sh4 = (tex2Dgrad( tNormalHeightMap, texSampleBase + vLightRayTS * 0.22, dx, dy ).a - sh0 - 0.22 ) * 12 * g_fShadowSoftening;
   
        // Compute the actual shadow strength:
        fOcclusionShadow = 1 - max( max( max( max( max( max( shA, sh9 ), sh8 ), sh7 ), sh6 ), sh5 ), sh4 );
      
        // The previous computation overbrightens the image, let's adjust for that:
        fOcclusionShadow = fOcclusionShadow * 0.6 + 0.4;         
     }       
   }   

   // Compute resulting color for the pixel:
   cResultColor = ComputeIllumination( texSample, vLightTS, vViewTS, fOcclusionShadow );
              
   if ( g_bVisualizeLOD )
   {
      cResultColor *= cLODColoring;
   }
   
   // Visualize currently computed mip level, tinting the color blue if we are in 
   // the region outside of the threshold level:
   if ( g_bVisualizeMipLevel )
   {
      cResultColor = fMipLevel.xxxx;      
   }   

   // If using HDR rendering, make sure to tonemap the resuld color prior to outputting it.
   // But since this example isn't doing that, we just output the computed result color here:
   return cResultColor;
}   


//--------------------------------------------------------------------------------------
// Bump mapping shader
//--------------------------------------------------------------------------------------
float4 RenderSceneBumpMapPS( PS_INPUT i ) : COLOR0
{ 
   //  Normalize the interpolated vectors:
   float3 vViewTS   = normalize( i.vViewTS  );
   float3 vLightTS  = normalize( i.vLightTS );
     
   float4 cResultColor = float4( 0, 0, 0, 1 );

   // Start the current sample located at the input texture coordinate, which would correspond
   // to computing a bump mapping result:
   float2 texSample = i.texCoord;

   // Compute resulting color for the pixel:
   cResultColor = ComputeIllumination( texSample, vLightTS, vViewTS, 1.0f );
              
   // If using HDR rendering, make sure to tonemap the resuld color prior to outputting it.
   // But since this example isn't doing that, we just output the computed result color here:
   return cResultColor;
}   


//--------------------------------------------------------------------------------------
// Apply parallax mapping with offset limiting technique to the current pixel
//--------------------------------------------------------------------------------------
float4 RenderSceneParallaxMappingPS( PS_INPUT i ) : COLOR0
{ 
   const float sfHeightBias = 0.01;
   
   //  Normalize the interpolated vectors:
   float3 vViewTS   = normalize( i.vViewTS  );
   float3 vLightTS  = normalize( i.vLightTS );
   
   // Sample the height map at the current texture coordinate:
   float fCurrentHeight = tex2D( tNormalHeightMap, i.texCoord ).a;
   
   // Scale and bias this height map value:
   float fHeight = fCurrentHeight * g_fHeightMapScale + sfHeightBias;
   
   // Perform offset limiting if desired:
   fHeight /= vViewTS.z;
   
   // Compute the offset vector for approximating parallax:
   float2 texSample = i.texCoord + vViewTS.xy * fHeight;
   
   float4 cResultColor = float4( 0, 0, 0, 1 );

   // Compute resulting color for the pixel:
   cResultColor = ComputeIllumination( texSample, vLightTS, vViewTS, 1.0f );
              
   // If using HDR rendering, make sure to tonemap the resuld color prior to outputting it.
   // But since this example isn't doing that, we just output the computed result color here:
   return cResultColor;
}   


//--------------------------------------------------------------------------------------
// Renders scene to render target
//--------------------------------------------------------------------------------------
technique RenderSceneWithPOM
{
    pass P0
    {          
        VertexShader = compile vs_3_0 RenderSceneVS();
        PixelShader  = compile ps_3_0 RenderScenePS(); 
    }
}

technique RenderSceneWithBumpMap
{
    pass P0
    {          
        VertexShader = compile vs_2_0 RenderSceneVS();
        PixelShader  = compile ps_2_0 RenderSceneBumpMapPS(); 
    }
}
technique RenderSceneWithPM
{
    pass P0
    {          
        VertexShader = compile vs_2_0 RenderSceneVS();
        PixelShader  = compile ps_2_0 RenderSceneParallaxMappingPS(); 
    }
}

#endif
