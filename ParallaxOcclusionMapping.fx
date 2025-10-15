//--------------------------------------------------------------------------------------
// File: ParallaxOcclusionMapping.fx
//
// �����Օ��}�b�s���O�iParallax Occlusion Mapping, POM�j�̎���
//
// ���̘_���Ő�������Ă���A���S���Y���̎����F
// "Dynamic Parallax Occlusion Mapping with Approximate Soft Shadows"
// ����: N. Tatarchuk�iATI Research�j
// ACM Symposium on Interactive 3D Graphics and Games, 2006 �f�ڗ\��
//
// �����ԃV�[���ł̎g�p��� ATI X1K �f�� "ToyShop" ���Q��:
//    http://www.ati.com/developer/demos/rx1800.html
//
// Copyright (c) ATI Research, Inc. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// �O���[�o���ϐ�
//--------------------------------------------------------------------------------------

texture g_baseTexture; // �x�[�X�J���[�i�A���x�h�j�e�N�X�`��
texture g_nmhTexture; // �@���}�b�v�{�����}�b�v�̕����e�N�X�`��

float4 g_materialAmbientColor; // �}�e���A���̊��F
float4 g_materialDiffuseColor; // �}�e���A���̊g�U���ːF
float4 g_materialSpecularColor; // �}�e���A���̋��ʔ��ːF

float g_fSpecularExponent; // ���ʃn�C���C�g�̎w��
bool g_bAddSpecular; // ���ʐ�����L�������邩�ǂ���

// �����p�����[�^:
float3 g_LightDir; // ���̕����i���[���h��ԁj
float4 g_LightDiffuse; // ���̊g�U�F
float4 g_LightAmbient; // ���̊��F

float4 g_vEye; // �J�����ʒu
float g_fBaseTextureRepeat; // �x�[�X�^�@���e�N�X�`���̃^�C�����O�W��
float g_fHeightMapScale; // �����}�b�v�̗L���Ȓl��i�X�P�[���j��\��

// �s��:
float4x4 g_mWorld; // �I�u�W�F�N�g�̃��[���h�s��
float4x4 g_mWorldViewProjection; // World * View * Projection �s��
float4x4 g_mView; // �r���[�s��

bool g_bVisualizeLOD; // LOD �̉����i�F�����j���s����
bool g_bVisualizeMipLevel; // �~�b�v���x���̉������s����
bool g_bDisplayShadows; // �Z���t�I�N���[�W�����Ɋ�Â��e��\�����邩

float2 g_vTextureDims = float2(512.f, 512.f); // ���s���Ƀ~�b�v���x�����v�Z���邽�߂�
                                    // �e�N�X�`�����@�i��, �����j���w��

int g_nLODThreshold; // �����Օ��}�b�s���O�i�t���v�Z�j����
                                    // �o���v�}�b�s���O�֐؂�ւ���~�b�v���x���������l

float g_fShadowSoftening; // �\�t�g�V���h�E�̂ڂ����W��

int g_nMinSamples; // �����v���t�@�C�����T���v�����O����ŏ��T���v����
int g_nMaxSamples; // �����v���t�@�C�����T���v�����O����ő�T���v����



//--------------------------------------------------------------------------------------
// �e�N�X�`���T���v��
//--------------------------------------------------------------------------------------
sampler tBase =
sampler_state
{
    Texture = <g_baseTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};
sampler tNormalHeightMap =
sampler_state
{
    Texture = <g_nmhTexture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};


//--------------------------------------------------------------------------------------
// ���_�V�F�[�_�o�͍\����
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 vLightTS : TEXCOORD1; // �ڋ�Ԃ̌��x�N�g���i���K�����Ă��Ȃ��j
    float3 vViewTS : TEXCOORD2; // �ڋ�Ԃ̎����x�N�g���i���K�����Ă��Ȃ��j
    float2 vParallaxOffsetTS : TEXCOORD3; // �ڋ�Ԃł̎����I�t�Z�b�g�x�N�g��
    float3 vNormalWS : TEXCOORD4; // ���[���h��Ԃ̖@��
    float3 vViewWS : TEXCOORD5; // ���[���h��Ԃ̎����x�N�g��
};


//--------------------------------------------------------------------------------------
// �W���I�ȕϊ��ƃ��C�e�B���O�ɕK�v�ȏ����v�Z
//--------------------------------------------------------------------------------------
VS_OUTPUT RenderSceneVS(float4 inPositionOS : POSITION,
                         float2 inTexCoord : TEXCOORD0,
                         float3 vInNormalOS : NORMAL,
                         float3 vInBinormalOS : BINORMAL,
                         float3 vInTangentOS : TANGENT)
{
    VS_OUTPUT Out;

    // �ʒu��ϊ����ďo��
    Out.position = mul(inPositionOS, g_mWorldViewProjection);

    // �e�N�X�`�����W��`�d�i�^�C���W����K�p�j
    Out.texCoord = inTexCoord * g_fBaseTextureRepeat;

    // �@���ETangent�EBinormal ���I�u�W�F�N�g��ԁ����[���h��Ԃ֕ϊ�
    float3 vNormalWS = mul(vInNormalOS, (float3x3) g_mWorld);
    float3 vTangentWS = mul(vInTangentOS, (float3x3) g_mWorld);
    float3 vBinormalWS = mul(vInBinormalOS, (float3x3) g_mWorld);

    // ���[���h��Ԃ̒��_�@�������̂܂܏o��
    Out.vNormalWS = vNormalWS;

    vNormalWS = normalize(vNormalWS);
    vTangentWS = normalize(vTangentWS);
    vBinormalWS = normalize(vBinormalWS);

    // ���[���h��Ԃł̈ʒu
    float4 vPositionWS = mul(inPositionOS, g_mWorld);

    // ���[���h��Ԃ̎����x�N�g���i���K�������j���v�Z���ďo��
    float3 vViewWS = g_vEye - vPositionWS;
    Out.vViewWS = vViewWS;

    // ���[���h��Ԃ̌��x�N�g���i���K�������j
    float3 vLightWS = g_LightDir;

    // ���E�����x�N�g���𐳋K�����A�ڋ�Ԃ֕ϊ�
    float3x3 mWorldToTangent = float3x3(vTangentWS, vBinormalWS, vNormalWS);

    // �ڋ�Ԃ̌��E�����x�N�g�����o��
    Out.vLightTS = mul(vLightWS, mWorldToTangent);
    Out.vViewTS = mul(mWorldToTangent, vViewWS);

    // �����v���t�@�C���Ǝ������C�̌����v�Z�ɗp���郌�C���������߂�
    // �i���o�͏�L�_�����Q�Ɓj

    // �����̏����I�t�Z�b�g�����i�ڋ�Ԃ� xy �𐳋K���j
    float2 vParallaxDirection = normalize(Out.vViewTS.xy);

    // ���̃x�N�g���̒������ő�ψʗʂ����߂�
    float fLength = length(Out.vViewTS);
    float fParallaxLength = sqrt(fLength * fLength - Out.vViewTS.z * Out.vViewTS.z) / Out.vViewTS.z;

    // ���ۂ́u�t�����v�����I�t�Z�b�g�x�N�g�����v�Z
    Out.vParallaxOffsetTS = vParallaxDirection * fParallaxLength;

    // �����}�b�v�̒l��̈Ⴂ��␳���邽�߁A�A�[�e�B�X�g�����p�̃X�P�[����K�p
    Out.vParallaxOffsetTS *= g_fHeightMapScale;

    return Out;
}


//--------------------------------------------------------------------------------------
// �s�N�Z���V�F�[�_�o�͍\����
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 RGBColor : COLOR0; // �o�͐F
};

struct PS_INPUT
{
    float2 texCoord : TEXCOORD0;
    float3 vLightTS : TEXCOORD1; // �ڋ�Ԃ̌��x�N�g���i���K�����Ă��Ȃ��j
    float3 vViewTS : TEXCOORD2; // �ڋ�Ԃ̎����x�N�g���i���K�����Ă��Ȃ��j
    float2 vParallaxOffsetTS : TEXCOORD3; // �ڋ�Ԃł̎����I�t�Z�b�g
    float3 vNormalWS : TEXCOORD4; // ���[���h��Ԃ̖@��
    float3 vViewWS : TEXCOORD5; // ���[���h��Ԃ̎����x�N�g��
};


//--------------------------------------------------------------------------------------
// �֐�:    ComputeIllumination
//
// ����:    �w��s�N�Z���̑����e�N�X�`���ƌ��x�N�g����p����
//          Phong ���̏Ɩ����v�Z����
//--------------------------------------------------------------------------------------
float4 ComputeIllumination(float2 texCoord, float3 vLightTS, float3 vViewTS, float fOcclusionShadow)
{
   // �@���}�b�v����@���i�ڋ�ԁj���T���v�����Đ��K��
    float3 vNormalTS = normalize(tex2D(tNormalHeightMap, texCoord) * 2 - 1);

   // �x�[�X�J���[���T���v��
    float4 cBaseColor = tex2D(tBase, texCoord);

   // �g�U���ː������v�Z
    float3 vLightTSAdj = float3(vLightTS.x, -vLightTS.y, vLightTS.z);

    float4 cDiffuse = saturate(dot(vNormalTS, vLightTSAdj)) * g_materialDiffuseColor;

   // �K�v�ł���΋��ʐ������v�Z
    float4 cSpecular = 0;
    if (g_bAddSpecular)
    {
        float3 vReflectionTS = normalize(2 * dot(vViewTS, vNormalTS) * vNormalTS - vViewTS);

        float fRdotL = saturate(dot(vReflectionTS, vLightTSAdj));
        cSpecular = saturate(pow(fRdotL, g_fSpecularExponent)) * g_materialSpecularColor;
    }

   // �ŏI�F������
    float4 cFinalColor = ((g_materialAmbientColor + cDiffuse) * cBaseColor + cSpecular) * fOcclusionShadow;

    return cFinalColor;
}


//--------------------------------------------------------------------------------------
// �����Օ��}�b�s���O�iPOM�j�̃s�N�Z���V�F�[�_
//
// ����: ���̃V�F�[�_�ɂ͋���ړI�̃��[�h���܂܂�Ă��܂��B
//       ���ۂ̃Q�[���╡�G�ȃV�[���ł́A���o�i���iLOD�A�X�y�L�����A�e�Ȃǁj��
//       �g�O�����鏈���͕ʂ̍œK�ȕ��@�ōs���ׂ��ł��B
//       �����ł͋��ނƂ��ĕ�����₷���������Ă��܂��B
//--------------------------------------------------------------------------------------
float4 RenderScenePS(PS_INPUT i) : COLOR0
{

   // ��Ԃ��ꂽ�x�N�g���𐳋K��
    float3 vViewTS = normalize(i.vViewTS);
    float3 vViewWS = normalize(i.vViewWS);
    float3 vLightTS = normalize(i.vLightTS);
    float3 vNormalWS = normalize(i.vNormalWS);

    float4 cResultColor = float4(0, 0, 0, 1);

   // �s�N�Z���V�F�[�_���Ń~�b�v���x���𖾎��I�Ɍv�Z���A
   // �t�� POM ����o���v�}�b�s���O�ւ� LOD �ؑւ��s���K�����̎����B
   // �ڍׂƗ��_�͑O�q�̘_�����Q�ƁB

   // ���݂̌��z���v�Z
    float2 fTexCoordsPerSize = i.texCoord * g_vTextureDims;

   // ddx/ddy �� x, y �̔�������x�Ɏ擾���œK��
    float2 dxSize, dySize;
    float2 dx, dy;

    float4(dxSize, dx) = ddx(float4(fTexCoordsPerSize, i.texCoord));
    float4(dySize, dy) = ddy(float4(fTexCoordsPerSize, i.texCoord));

    float fMipLevel;
    float fMipLevelInt; // �~�b�v���x���̐�����
    float fMipLevelFrac; // �~�b�v���x���̏������i��ԂɎg�p�j

    float fMinTexCoordDelta;
    float2 dTexCoords;

   // u �� v �̕ω��ʂ̓��a�i�}�C�i���[�u�j
    dTexCoords = dxSize * dxSize + dySize * dySize;

   // �W���I�ȃ~�b�v�}�b�s���O�ł� max ��p����
    fMinTexCoordDelta = max(dTexCoords.x, dTexCoords.y);

   // ���݂̃~�b�v���x�����v�Z�i*0.5 �͎����I�� sqrt �ɑ����j
    fMipLevel = max(0.5 * log2(fMinTexCoordDelta), 0);

   // �܂��͓��͂̃e�N�X�`�����W�ŃT���v���i=�o���v�}�b�v�����j
    float2 texSample = i.texCoord;

   // LOD ���o���̂��߂̏�Z�F�ig_nLODThreshold �̐����Q�Ɓj
    float4 cLODColoring = float4(1, 1, 3, 1);

    float fOcclusionShadow = 1.0;

    if (fMipLevel <= (float) g_nLODThreshold)
    {
      //===============================================//
      // �����Օ��}�b�s���O�̃I�t�Z�b�g�v�Z          //
      //===============================================//

      // ���I�t���[����ɂ��A���p�ɉ����ăT���v������ύX�B
      // �΂ߎ��قǃX�e�b�v���ׂ������Đ��x���グ��B
      // �􉽖@���Ǝ����̊p�x�ɑ΂����`�ɃT���v�����x��ς���B
        int nNumSteps = (int) lerp(g_nMaxSamples, g_nMinSamples, dot(vViewWS, vNormalWS));

      // �����I�t�Z�b�g�����i���_�V�F�[�_�Ōv�Z�j�ɉ����āA
      // �����v���t�@�C���Ǝ������C�̌�����������B
      // ���̃R�[�h�� HLSL �̓��I�t���[����ɓ������Ă���A�\���ɕq���B
      // ���̗�ֈڐA���Ă����I������ێ��������ꍇ�͒��ӂ��K�v�B
      //
      // �ȉ��ł͍����v���t�@�C�����敪���`�ŋߎ�����B
      // ���C�ƃv���t�@�C���̌�_���܂�2�_�������A
      // ����2�_�Œ�������ƃ��C�̌������v�Z���ăI�t�Z�b�g�𓾂�B
      // �ڍׂ͑O�q�̘_�����Q�ƁB
      //

        float fCurrHeight = 0.0;
        float fStepSize = 1.0 / (float) nNumSteps;
        float fPrevHeight = 1.0;
        float fNextHeight = 0.0;

        int nStepIndex = 0;
        bool bCondition = true;

        float2 vTexOffsetPerStep = fStepSize * i.vParallaxOffsetTS;
        float2 vTexCurrentOffset = i.texCoord;
        float fCurrentBound = 1.0;
        float fParallaxAmount = 0.0;

        float2 pt1 = 0;
        float2 pt2 = 0;

        float2 texOffset2 = 0;

        while (nStepIndex < nNumSteps)
        {
            vTexCurrentOffset -= vTexOffsetPerStep;

         // �����}�b�v���T���v���i�{��ł͖@���}�b�v�̃A���t�@�Ɋi�[�j
            fCurrHeight = tex2Dgrad(tNormalHeightMap, vTexCurrentOffset, dx, dy).a;

            fCurrentBound -= fStepSize;

            if (fCurrHeight > fCurrentBound)
            {
                pt1 = float2(fCurrentBound, fCurrHeight);
                pt2 = float2(fCurrentBound + fStepSize, fPrevHeight);

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

      // SM3.0 �ł� 0 ���Z�� Inf �𐶐����Ă��܂����߁A�`�F�b�N���K�v
        if (fDenominator == 0.0f)
        {
            fParallaxAmount = 0.0f;
        }
        else
        {
            fParallaxAmount = (pt1.x * fDelta2 - pt2.x * fDelta1) / fDenominator;
        }

        float2 vParallaxOffset = i.vParallaxOffsetTS * (1 - fParallaxAmount);

      // �^���I�ɉ����o���ꂽ�\�ʏ�̍ŏI�e�N�X�`�����W
        float2 texSampleBase = i.texCoord - vParallaxOffset;
        texSample = texSampleBase;

      // 臒l�t�߁i�J�ڗ̈�j�̏ꍇ�̂݃o���v�}�b�v�֕��
        cLODColoring = float4(1, 1, 1, 1);

        if (fMipLevel > (float) (g_nLODThreshold - 1))
        {
         // �������ŕ�Ԋ���������
            fMipLevelFrac = modf(fMipLevel, fMipLevelInt);

            if (g_bVisualizeLOD)
            {
            // �����p�FPOM���ʁ��F�i�J�ڑw�j�֕�ԕ\��
                cLODColoring = float4(1, 1, max(1, 2 * fMipLevelFrac), 1);
            }

         // POM ���W����o���v�}�b�v���W�ցA���݂̃~�b�v���x���ɉ����Ċ��炩�ɕ��
            texSample = lerp(texSampleBase, i.texCoord, fMipLevelFrac);
        }

        if (g_bDisplayShadows == true)
        {
            float2 vLightRayTS = vLightTS.xy * g_fHeightMapScale;

        // �����v���t�@�C���̃Z���t�I�N���[�W�������l������
        // �\�t�g�łڂ����e���v�Z
            float sh0 = tex2Dgrad(tNormalHeightMap, texSampleBase, dx, dy).a;
            float shA = (tex2Dgrad(tNormalHeightMap, texSampleBase + vLightRayTS * 0.88, dx, dy).a - sh0 - 0.88) * 1 * g_fShadowSoftening;
            float sh9 = (tex2Dgrad(tNormalHeightMap, texSampleBase + vLightRayTS * 0.77, dx, dy).a - sh0 - 0.77) * 2 * g_fShadowSoftening;
            float sh8 = (tex2Dgrad(tNormalHeightMap, texSampleBase + vLightRayTS * 0.66, dx, dy).a - sh0 - 0.66) * 4 * g_fShadowSoftening;
            float sh7 = (tex2Dgrad(tNormalHeightMap, texSampleBase + vLightRayTS * 0.55, dx, dy).a - sh0 - 0.55) * 6 * g_fShadowSoftening;
            float sh6 = (tex2Dgrad(tNormalHeightMap, texSampleBase + vLightRayTS * 0.44, dx, dy).a - sh0 - 0.44) * 8 * g_fShadowSoftening;
            float sh5 = (tex2Dgrad(tNormalHeightMap, texSampleBase + vLightRayTS * 0.33, dx, dy).a - sh0 - 0.33) * 10 * g_fShadowSoftening;
            float sh4 = (tex2Dgrad(tNormalHeightMap, texSampleBase + vLightRayTS * 0.22, dx, dy).a - sh0 - 0.22) * 12 * g_fShadowSoftening;

        // ���ۂ̉e���x���v�Z
            fOcclusionShadow = 1 - max(max(max(max(max(max(shA, sh9), sh8), sh7), sh6), sh5), sh4);

        // ��L�v�Z�͂�▾�邭�Ȃ肪���Ȃ̂ŕ␳
            fOcclusionShadow = fOcclusionShadow * 0.6 + 0.4;
        }
    }

   // �s�N�Z���̍ŏI�F���v�Z
    cResultColor = ComputeIllumination(texSample, vLightTS, vViewTS, fOcclusionShadow);

    if (g_bVisualizeLOD)
    {
        cResultColor *= cLODColoring;
    }

   // �~�b�v���x���̉����F臒l�O�̗̈�ł͐n�ɒ��F
    if (g_bVisualizeMipLevel)
    {
        cResultColor = fMipLevel.xxxx;
    }

   // HDR ���g���ꍇ�͏o�͑O�Ƀg�[���}�b�v���K�v�B
   // ���̗�ł͍s���Ă��Ȃ����߁A�v�Z�����F�����̂܂܏o�͂���B
    return cResultColor;
}


//--------------------------------------------------------------------------------------
// �o���v�}�b�s���O�p�V�F�[�_
//--------------------------------------------------------------------------------------
float4 RenderSceneBumpMapPS(PS_INPUT i) : COLOR0
{
   // ��ԃx�N�g���̐��K��
    float3 vViewTS = normalize(i.vViewTS);
    float3 vLightTS = normalize(i.vLightTS);

    float4 cResultColor = float4(0, 0, 0, 1);

   // ���� UV �ŃT���v���i�o���v�}�b�v�����j
    float2 texSample = i.texCoord;

   // �ŏI�F���v�Z
    cResultColor = ComputeIllumination(texSample, vLightTS, vViewTS, 1.0f);

   // HDR �̏ꍇ�͏o�͑O�Ƀg�[���}�b�v���K�v�B
   // �{��ł͂��̂܂܏o�́B
    return cResultColor;
}


//--------------------------------------------------------------------------------------
// �I�t�Z�b�g�E���~�b�g�����ɂ��p�����b�N�X�}�b�s���O��K�p
//--------------------------------------------------------------------------------------
float4 RenderSceneParallaxMappingPS(PS_INPUT i) : COLOR0
{
    const float sfHeightBias = 0.01;

   // ��ԃx�N�g���̐��K��
    float3 vViewTS = normalize(i.vViewTS);
    float3 vLightTS = normalize(i.vLightTS);

   // ���݂̃e�N�X�`�����W�ō����}�b�v���T���v��
    float fCurrentHeight = tex2D(tNormalHeightMap, i.texCoord).a;

   // �X�P�[���ƃo�C�A�X��K�p
    float fHeight = fCurrentHeight * g_fHeightMapScale + sfHeightBias;

   // �K�v�Ȃ�I�t�Z�b�g�E���~�b�g�����{
    fHeight /= vViewTS.z;

   // �����ߎ��̃I�t�Z�b�g�x�N�g�����v�Z
    float2 texSample = i.texCoord + vViewTS.xy * fHeight;

    float4 cResultColor = float4(0, 0, 0, 1);

   // �ŏI�F���v�Z
    cResultColor = ComputeIllumination(texSample, vLightTS, vViewTS, 1.0f);

   // HDR �̏ꍇ�͏o�͑O�Ƀg�[���}�b�v���K�v�B
   // �{��ł͂��̂܂܏o�́B
    return cResultColor;
}


//--------------------------------------------------------------------------------------
// �V�[���������_�[�^�[�Q�b�g�֕`��
//--------------------------------------------------------------------------------------
technique RenderSceneWithPOM
{
    pass P0
    {
        VertexShader = compile vs_3_0 RenderSceneVS();
        PixelShader = compile ps_3_0 RenderScenePS();
    }
}

technique RenderSceneWithBumpMap
{
    pass P0
    {
        VertexShader = compile vs_2_0 RenderSceneVS();
        PixelShader = compile ps_2_0 RenderSceneBumpMapPS();
    }
}
technique RenderSceneWithPM
{
    pass P0
    {
        VertexShader = compile vs_2_0 RenderSceneVS();
        PixelShader = compile ps_2_0 RenderSceneParallaxMappingPS();
    }
}
