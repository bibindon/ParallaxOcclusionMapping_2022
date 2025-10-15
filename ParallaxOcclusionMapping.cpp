//=================================================================================================
// File: ParallaxOcclusionMapping.cpp
//
// Author: Natalya Tatarchuk
//         ATI Research, Inc.
//         3D Application Research Group
//
// "Dynamic Parallax Occlusion Mapping with Approximate Soft Shadows"
//（N. Tatarchuk, ATI Research）で説明されているアルゴリズムの実装。
//本論文は ACM Symposium on Interactive 3D Graphics and Games, 2006 に掲載予定。
//
// Copyright (c) ATI Research, Inc. All rights reserved.
//=================================================================================================

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"

#pragma comment(lib, "legacy_stdio_definitions.lib")

//#define DEBUG_VS   // 頂点シェーダのデバッグを有効にするにはコメントアウトを外す
//#define DEBUG_PS   // ピクセルシェーダのデバッグを有効にするにはコメントアウトを外す


//--------------------------------------------------------------------------------------
// グローバル変数・パラメータ
//--------------------------------------------------------------------------------------
const int                   s_iMAX_STRING_SIZE = 100;
const int                   s_iDECL_SIZE = 56;
const int                   s_iNUM_TEXTURES = 7;  // このサンプルで読み込むテクスチャの数

//--------------------------------------------------------------------------------------
// このサンプルで使用するテクスチャペアのID
//--------------------------------------------------------------------------------------
enum POM_TEXTURE_IDS
{
    POM_STONES = 0,
    POM_ROCKS,
    POM_WALL,
    POM_FOUR_BUMPS,
    POM_BUMPS,
    POM_DENTS,
    POM_SAINT
};

//--------------------------------------------------------------------------------------
// 使用する描画テクニックのID
//--------------------------------------------------------------------------------------
enum TECHNIQUE_IDS
{
    POM,           // Parallax Occlusion Mapping
    BUMPMAP,       // Bump Mapping
    PM             // Offset Limiting を用いた Parallax Mapping
};

int                         g_nCurrentTextureID = POM_STONES;
int                         g_nCurrentTechniqueID = POM;

//--------------------------------------------------------------------------------------
// このサンプルで使用するテクスチャペアのファイル名
//--------------------------------------------------------------------------------------
WCHAR*                      g_strBaseTextureNames[] =
{
    TEXT( "stones.bmp" ),
    TEXT( "rocks.jpg" ),
    TEXT( "wall.jpg" ),
    TEXT( "wood.jpg" ),
    TEXT( "concrete.bmp" ),
    TEXT( "concrete.bmp" ),
    TEXT( "wood.jpg" )
};

WCHAR*                      g_strNMHTextureNames[] =
{
    TEXT( "stones_NM_height.tga" ),
    TEXT( "rocks_NM_height.tga" ),
    TEXT( "wall_NM_height.tga" ),
    TEXT( "four_NM_height.tga" ),
    TEXT( "bump_NM_height.tga" ),
    TEXT( "dent_NM_height.tga" ),
    TEXT( "saint_NM_height.tga" )

};


//--------------------------------------------------------------------------------------
IDirect3DDevice9*           g_pD3DDevice = NULL;       // 現在レンダリングに使用している D3D デバイス

ID3DXFont*                  g_pFont = NULL;            // テキスト描画用フォント
ID3DXSprite*                g_pSprite = NULL;          // テキスト描画をまとめるスプライト
bool                        g_bShowHelp = false;       // true のとき UI ヘルプを描画する
CModelViewerCamera          g_Camera;                  // モデルビュー用カメラ
ID3DXEffect*                g_pEffect = NULL;          // D3DX エフェクト インターフェース

ID3DXMesh*                  g_pMesh = NULL;            // メッシュオブジェクト

IDirect3DTexture9**         g_pBaseTextures = NULL;    // ベースカラー テクスチャ
IDirect3DTexture9**         g_pNMHTextures = NULL;     // 法線マップ／高さマップの複合テクスチャ
                                                        //  高さはアルファチャンネルに格納

CDXUTDialogResourceManager  g_DialogResourceManager;   // ダイアログの共有リソース管理
CD3DSettingsDlg             g_SettingsDlg;             // デバイス設定ダイアログ
CDXUTDialog                 g_HUD;                     // 3D UI を管理
CDXUTDialog                 g_SampleUI;                // サンプル専用 UI
D3DXMATRIXA16               g_mWorldFix;

CDXUTDirectionWidget        g_LightControl;            // シーンのライト制御
float                       g_fLightScale;             // ライトの明るさスケール


//--------------------------------------------------------------------------------------
// UI 用スカラー
//--------------------------------------------------------------------------------------
const float                 s_fLightScaleUIScale = 10.0f;
const int                   s_iLightScaleSliderMin = 0;
const int                   s_iLightScaleSliderMax = 20;

int                         g_iHeightScaleSliderMin = 0;
int                         g_iHeightScaleSliderMax = 100;
float                       g_fHeightScaleUIScale = 100.0f;

const int                   s_iSliderMin = 8;
const int                   s_iSliderMax = 130;

//--------------------------------------------------------------------------------------
// マテリアルプロパティ
//--------------------------------------------------------------------------------------
D3DXCOLOR                   g_colorMtrlDiffuse( 1.0f, 1.0f, 1.0f, 1.0f );
D3DXCOLOR                   g_colorMtrlAmbient( 0.35f, 0.35f, 0.35f, 0 );
D3DXCOLOR                   g_colorMtrlSpecular( 1.0f, 1.0f, 1.0f, 1.0f );

float                       g_fSpecularExponent( 60.0f );        // マテリアルの鏡面指数
float                       g_fBaseTextureRepeat( 1.0f );        // ベース／法線テクスチャのタイリング係数

bool                        g_bVisualizeLOD( false );            // LOD 可視化（色分け）の ON/OFF
bool                        g_bVisualizeMipLevel( false );       // ミップレベル可視化の ON/OFF
bool                        g_bDisplayShadows( false );          // セルフオクルージョンによる影の ON/OFF
bool                        g_bAddSpecular( false );             // スペキュラの ON/OFF
bool                        g_bRenderPOM( true );                // POM を使うか（通常は使用）

int                         g_nLODThreshold( 9 );                // 視差遮蔽マッピング（フル計算）から
                                                                 // バンプマップへ切り替えるミップレベル
int                         g_nMinSamples( 8 );                  // 高さプロファイルをサンプリングする最小回数
int                         g_nMaxSamples( 50 );                 // 高さプロファイルをサンプリングする最大回数
float                       g_fShadowSoftening( 0.58f );         // ソフトシャドウのぼかし係数
float                       g_fHeightScale;                      // 高さマップの有効レンジ（スケール）

//--------------------------------------------------------------------------------------
// 使用するメッシュファイル名
//--------------------------------------------------------------------------------------
WCHAR*                      g_strMeshFileName0 = TEXT( "Disc.x" );


//--------------------------------------------------------------------------------------
// UI コントロール ID
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN           1
#define IDC_TOGGLEREF                  3
#define IDC_CHANGEDEVICE               4
#define IDC_HEIGHT_SCALE_SLIDER        5
#define IDC_HEIGHT_SCALE_STATIC        6
#define IDC_TOGGLE_SHADOWS             7
#define IDC_SELECT_TEXTURES_COMBOBOX   8
#define IDC_TOGGLE_SPECULAR            9
#define IDC_SPECULAR_EXPONENT_SLIDER   10
#define IDC_SPECULAR_EXPONENT_STATIC   11
#define IDC_MIN_NUM_SAMPLES_SLIDER     12
#define IDC_MIN_NUM_SAMPLES_STATIC     13
#define IDC_MAX_NUM_SAMPLES_SLIDER     14
#define IDC_MAX_NUM_SAMPLES_STATIC     15
#define IDC_TECHNIQUE_COMBO_BOX        16
#define IDC_RELOAD_BUTTON              20


//--------------------------------------------------------------------------------------
// 先行宣言
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText( double fTime );


//--------------------------------------------------------------------------------------
// エントリポイント。初期化したのちメッセージループに入る。
// アイドル時間にシーンをレンダリングする。
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
    // デバッグビルドでのランタイムメモリチェックを有効化
#if defined(DEBUG) | defined(_DEBUG)
      _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
   #endif

    // 各種コールバックを設定
    DXUTSetCallbackD3D9DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCursorSettings( true, true );
    InitApp();
    DXUTInit( true, true ); // コマンドラインを解析しメッセージボックスを表示
    DXUTSetHotkeyHandling( true, true, true );  // 既定のホットキーを処理
    DXUTCreateWindow( L"Parallax Occlusion Mapping" );
    DXUTCreateDevice( true, 800, 600 );
    DXUTMainLoop();

    return DXUTGetExitCode();

}

//--------------------------------------------------------------------------------------
// 指定されたテクスチャインデックスに基づき、ベースと法線/高さテクスチャを選択。
// 併せて高さマップのスケールなど関連パラメータを設定。
// 注意: g_pBaseTextures / g_pNMHTextures のテクスチャは事前に作成済みであること。
//--------------------------------------------------------------------------------------
void SetPOMTextures( int iTextureIndex )
{
    g_nCurrentTextureID = iTextureIndex;

    // シェーダの該当スロットに新しいテクスチャをバインド
    if( g_pD3DDevice && g_pEffect )
    {
        HRESULT hr;

        V( g_pEffect->SetTexture( "g_baseTexture", g_pBaseTextures[g_nCurrentTextureID] ) );
        V( g_pEffect->SetTexture( "g_nmhTexture", g_pNMHTextures[g_nCurrentTextureID] ) );

    }

    // テクスチャごとに高さマップのレンジが異なるため、スライダーの範囲を設定
    switch( iTextureIndex )
    {
        case POM_STONES:
        {
            // Stones テクスチャペア
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.02f;

            g_fSpecularExponent = 60.0f;
            g_fBaseTextureRepeat = 1.0f;

            g_nMinSamples = 8;
            g_nMaxSamples = 50;
            break;
        }

        case POM_ROCKS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.1f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 1.0f;

            g_nMinSamples = 8;
            g_nMaxSamples = 100;
            break;
        }

        case POM_WALL:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.06f;

            g_fSpecularExponent = 60.0f;
            g_fBaseTextureRepeat = 1.0f;

            g_nMinSamples = 8;
            g_nMaxSamples = 50;
            break;
        }

        case POM_FOUR_BUMPS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 10.0f;
            g_fHeightScale = 0.2f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 1.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }

        case POM_BUMPS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 10.0f;
            g_fHeightScale = 0.2f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 4.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }

        case POM_DENTS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 10.0f;
            g_fHeightScale = 0.2f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 4.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }

        case POM_SAINT:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.08f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 1.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// アプリ初期化：ライト制御と各種 GUI を初期化
//--------------------------------------------------------------------------------------
void InitApp()
{
    // UI レイアウト用パラメータ
    const int ciPadding = 24;        // 各コントロール間の縦方向パディング
    const int ciLeft = 5;            // 左端位置
    const int ciWidth = 125;         // ウィジェット幅
    const int ciHeight = 22;         // ウィジェット高さ

    // ライトの初期化
    g_LightControl.SetLightDirection( D3DXVECTOR3( sinf( D3DX_PI * 2 - D3DX_PI / 6 ),
                                                   0,
                                                   -cosf( D3DX_PI * 2 - D3DX_PI / 6 ) ) );
    g_fLightScale = 1.0f;

    // POM テクスチャと高さパラメータの初期化
    SetPOMTextures( POM_STONES );

    // ダイアログ初期化 //
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    // UI 要素の初期化 //

    // 上段トグルボタン //
    g_HUD.SetCallback( OnGUIEvent );

    int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", ciLeft, iY, ciWidth, ciHeight );
    iY += ciPadding;
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", ciLeft, iY, ciWidth, ciHeight );
    iY += ciPadding;
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", ciLeft, iY, ciWidth, ciHeight, VK_F2 );
    iY += ciPadding;

    // Reload ボタンはデバッグ時のみ有効
#ifdef _DEBUG
      g_HUD.AddButton( IDC_RELOAD_BUTTON, L"Reload effect", ciLeft, iY, ciWidth, ciHeight );
   #endif

    iY = 10;
    g_SampleUI.SetCallback( OnGUIEvent );

    WCHAR sz[s_iMAX_STRING_SIZE];

    // スライダーの位置
    const int ciSliderLeft = 15;
    const int ciSliderWidth = 100;

    // テクニック選択コンボボックス
    CDXUTComboBox* pTechniqueComboBox = NULL;
    g_SampleUI.AddComboBox( IDC_TECHNIQUE_COMBO_BOX, ciLeft, iY, 200, 24, L'O', false, &pTechniqueComboBox );
    if( pTechniqueComboBox )
    {
        pTechniqueComboBox->SetDropHeight( 100 );
        pTechniqueComboBox->AddItem( L"Parallax Occlusion Mapping", ( LPVOID )POM );
        pTechniqueComboBox->AddItem( L"Bump Mapping", ( LPVOID )BUMPMAP );
        pTechniqueComboBox->AddItem( L"Parallax Mapping with Offset Limiting", ( LPVOID )PM );

    }

    // Height scale（高さレンジ）設定
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Height scale: %0.2f", g_fHeightScale );
    g_SampleUI.AddStatic( IDC_HEIGHT_SCALE_STATIC, sz, ciLeft, iY, ciWidth, ciHeight );

    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_HEIGHT_SCALE_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight, g_iHeightScaleSliderMin,
                          g_iHeightScaleSliderMax,
                          ( int )( g_fHeightScale * g_fHeightScaleUIScale ) );

    // テクスチャ選択コンボボックス
    iY += ciPadding;
    CDXUTComboBox* pTextureComboBox = NULL;
    g_SampleUI.AddComboBox( IDC_SELECT_TEXTURES_COMBOBOX, ciLeft, iY, 200, 24, L'O', false, &pTextureComboBox );
    if( pTextureComboBox )
    {
        pTextureComboBox->SetDropHeight( 100 );
        pTextureComboBox->AddItem( L"Stones", ( LPVOID )POM_STONES );
        pTextureComboBox->AddItem( L"Rocks", ( LPVOID )POM_ROCKS );
        pTextureComboBox->AddItem( L"Wall", ( LPVOID )POM_WALL );
        pTextureComboBox->AddItem( L"Sphere, Triangle, Torus, Pyramid", ( LPVOID )POM_FOUR_BUMPS );
        pTextureComboBox->AddItem( L"Bumps", ( LPVOID )POM_BUMPS );
        pTextureComboBox->AddItem( L"Dents", ( LPVOID )POM_DENTS );
        pTextureComboBox->AddItem( L"Relief", ( LPVOID )POM_SAINT );

    }

    // 影の ON/OFF
    iY += ciPadding;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_SHADOWS, L"Toggle self-occlusion shadows rendering: ON",
                            ciLeft, iY, 350, 24, g_bDisplayShadows, L'C', false );

    // スペキュラの ON/OFF
    iY += ciPadding;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_SPECULAR, L"Toggle specular: ON",
                            ciLeft, iY, 350, 24, g_bAddSpecular, L'C', false );


    // スペキュラ指数スライダー
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Specular exponent: %0.0f", g_fSpecularExponent );
    g_SampleUI.AddStatic( IDC_SPECULAR_EXPONENT_STATIC, sz, ciLeft, iY, ciWidth, ciHeight );

    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_SPECULAR_EXPONENT_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight,
                          s_iSliderMin, s_iSliderMax, ( int )g_fSpecularExponent );

    // サンプル数（最小）
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Minium number of samples: %d", g_nMinSamples );
    g_SampleUI.AddStatic( IDC_MIN_NUM_SAMPLES_STATIC, sz, ciLeft, iY, ciWidth * 2, ciHeight );
    g_SampleUI.GetStatic( IDC_MIN_NUM_SAMPLES_STATIC )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP |
        DT_WORDBREAK;

    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_MIN_NUM_SAMPLES_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight,
                          s_iSliderMin, s_iSliderMax, g_nMinSamples );

    // サンプル数（最大）
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Maximum number of samples: %d", g_nMaxSamples );
    g_SampleUI.AddStatic( IDC_MAX_NUM_SAMPLES_STATIC, sz, ciLeft, iY, ciWidth * 2, ciHeight );
    g_SampleUI.GetStatic( IDC_MAX_NUM_SAMPLES_STATIC )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP |
        DT_WORDBREAK;


    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_MAX_NUM_SAMPLES_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight,
                          s_iSliderMin, s_iSliderMax, g_nMaxSamples );

}


//--------------------------------------------------------------------------------------
// デバイス初期化時に呼ばれる。最低限の機能を満たしているかをチェックし、
// 条件を満たさない場合は E_FAIL を返す。
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps,
                                  D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat,
                                  bool bWindowed,
                                  void* pUserContext )
{
    // このサンプルの ParallaxOcclusionMapping テクニックには PS 3.0 が必要。
    // 少なくとも PS 3.0 をサポートしないデバイスは拒否する。
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 3, 0 ) )
    {
        return false;
    }

    // アルファブレンディングをサポートしないバックバッファフォーマットはスキップ
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal,
                                         pCaps->DeviceType,
                                         AdapterFormat,
                                         D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE,
                                         BackBufferFormat ) ) )
    {
        return false;
    }

    return true;

}


//--------------------------------------------------------------------------------------
// D3D9 デバイス作成直前に呼ばれる。必要に応じてデバイス設定を変更できる。
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings,
                                    void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    HRESULT hr;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 caps;

    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
                            pDeviceSettings->d3d9.DeviceType,
                            &caps ) );

    // VS 3.0 非対応デバイスの場合は SWVP（ソフトウェア頂点処理）に切り替える
    if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF &&
        ( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 ||
            caps.VertexShaderVersion < D3DVS_VERSION( 3, 0 ) ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // 頂点シェーダのデバッグには REF か SWVP が必要、
    // ピクセルシェーダのデバッグには REF が必要。
#ifdef DEBUG_VS
      if ( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
      {
         pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
         pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
         pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
      }
   #endif
#ifdef DEBUG_PS
      pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
   #endif

    // VSync を無効化
    pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // 最初に作成するデバイスが REF の場合は警告ダイアログを表示（任意）
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}

//--------------------------------------------------------------------------------------
// エフェクトファイルをディスクから読み込む。
// 呼び出し前に D3D デバイスが作成・設定済みであること。
//--------------------------------------------------------------------------------------
HRESULT LoadEffectFile()
{
    SAFE_RELEASE( g_pEffect );

    if( g_pD3DDevice == NULL )
        return E_FAIL;

    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;

#if defined( DEBUG ) || defined( _DEBUG )
    // D3DXSHADER_DEBUG を指定するとシェーダ内にデバッグ情報が埋め込まれる。
    // デバッグ体験は向上するが、最適化と実行結果はリリース相当と同じ。
    dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif

#ifdef DEBUG_VS
      dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
   #endif
#ifdef DEBUG_PS
      dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
   #endif

    WCHAR str[MAX_PATH];
    HRESULT hr;
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ParallaxOcclusionMapping.fx" ) );

    ID3DXBuffer* pErrors = NULL;
    hr = D3DXCreateBuffer( 1024, &pErrors );

    hr = D3DXCreateEffectFromFile( g_pD3DDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect, &pErrors );

    if( FAILED( hr ) )
    {
        // シェーダのコンパイルエラーをシェルに出力
        CHAR* pErrorStr = ( CHAR* )pErrors->GetBufferPointer();
        printf( "%s\n", pErrorStr );

        return E_FAIL;
    }

    SAFE_RELEASE( pErrors );

    return S_OK;

}

//--------------------------------------------------------------------------------------
// デバイスリセット後も生存する D3D9 リソース（D3DPOOL_MANAGED）を作成。
// バックバッファサイズに依存しないもののみ作成する。
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice,
                                 const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    g_pD3DDevice = pd3dDevice;

    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    // フォントを初期化
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    // メッシュを読み込む
    V_RETURN( LoadMesh( pd3dDevice, g_strMeshFileName0, &g_pMesh ) );

    D3DXVECTOR3* pData;
    D3DXVECTOR3 vCenter;
    FLOAT fObjectRadius;
    V( g_pMesh->LockVertexBuffer( 0, ( LPVOID* )&pData ) );
    V( D3DXComputeBoundingSphere( pData, g_pMesh->GetNumVertices(), s_iDECL_SIZE, &vCenter, &fObjectRadius ) );
    V( g_pMesh->UnlockVertexBuffer() );

    // 開始フレームでわずかに手前・斜めから見えるようメッシュを整列。
    // グレージング角を観察しやすくする。
    D3DXMatrixTranslation( &g_mWorldFix, -vCenter.x, -vCenter.y, -vCenter.z );
    D3DXMATRIXA16 mRotation;
    D3DXMatrixRotationY( &mRotation, -D3DX_PI / 3.0f );
    g_mWorldFix *= mRotation;
    D3DXMatrixRotationX( &mRotation, D3DX_PI / 3.0f );
    g_mWorldFix *= mRotation;

    // ライト制御の初期化
    V_RETURN( CDXUTDirectionWidget::StaticOnD3D9CreateDevice( pd3dDevice ) );
    g_LightControl.SetRadius( fObjectRadius );

    V_RETURN( LoadEffectFile() );

    // 本プログラムで使用する全テクスチャを読み込む
    WCHAR str[MAX_PATH];

    g_pBaseTextures = ( IDirect3DTexture9** )malloc( sizeof( IDirect3DTexture9* ) * s_iNUM_TEXTURES );
    if( g_pBaseTextures == NULL )
    {
        // ベーステクスチャ配列の確保に失敗
        printf( "ERROR allocating the array for base texture pointers storage!\n" );
        return E_FAIL;
    }
    g_pNMHTextures = ( IDirect3DTexture9** )malloc( sizeof( IDirect3DTexture9* ) * s_iNUM_TEXTURES );
    if( g_pNMHTextures == NULL )
    {
        // 法線／高さテクスチャ配列の確保に失敗
        printf( "ERROR allocating the array for normal map / height map texture pointers storage!\n" );
        return E_FAIL;
    }

    for( int iTextureIndex = POM_STONES; iTextureIndex < s_iNUM_TEXTURES; iTextureIndex++ )
    {
        // 各種 POM テクスチャペアごとに、ベースと法線/高さのテクスチャを読み込む

        // ベーステクスチャを読み込み
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_strBaseTextureNames[iTextureIndex] ) );

        V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str, D3DX_DEFAULT, D3DX_DEFAULT,
                                               D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                               D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                               NULL, NULL, &g_pBaseTextures[iTextureIndex] ) );

        // 法線/高さテクスチャを読み込み
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_strNMHTextureNames[iTextureIndex] ) );

        V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str, D3DX_DEFAULT, D3DX_DEFAULT,
                                               D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                               D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                               NULL, NULL, &g_pNMHTextures[iTextureIndex] ) );
    }

    SetPOMTextures( g_nCurrentTextureID );

    // カメラのビュー設定
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -15.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 0.5f, fObjectRadius * 10.0f );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// メッシュを読み込み、法線が無ければ生成する。
// さらに GPU の頂点キャッシュに最適化して、キャッシュミスを減らす。
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh )
{
    ID3DXMesh* pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    //====================================================================//
    // D3DX でメッシュを読み込み ID3DXMesh* を得る。                      //
    // このサンプルでは読み込むモデルが分かっているので、Xファイルの      //
    // 埋め込みマテリアルは無視する。一般的な読み込みは "OptimizedMesh"   //
    // などのサンプルを参照。                                             //
    //====================================================================//
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName ) );
    V_RETURN( D3DXLoadMeshFromX( str, D3DXMESH_MANAGED, pd3dDevice, NULL, NULL, NULL, NULL, &pMesh ) );

    // 必要な要素を含む頂点宣言を作成
    const D3DVERTEXELEMENT9 vertexDecl[] =
    {
        { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT,  0 },
        { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
        D3DDECL_END()
    };

    LPD3DXMESH pTempMesh = NULL;

    // 指定の宣言に合わせてクローン
    if( FAILED( pMesh->CloneMesh( pMesh->GetOptions(), vertexDecl, pd3dDevice, &pTempMesh ) ) )
    {
        SAFE_RELEASE( pTempMesh );
        return E_FAIL;
    }

    //====================================================================//
    // 元の宣言に法線／Tangent／Binormal が含まれていたかを確認           //
    //====================================================================//
    bool bHadNormal = false;
    bool bHadTangent = false;
    bool bHadBinormal = false;

    D3DVERTEXELEMENT9 vertexOldDecl[ MAX_FVF_DECL_SIZE ];

    if( pMesh && SUCCEEDED( pMesh->GetDeclaration( vertexOldDecl ) ) )
    {
        // 宣言を走査して該当チャンネルを探す
        for( UINT iChannelIndex = 0; iChannelIndex < D3DXGetDeclLength( vertexOldDecl ); iChannelIndex++ )
        {
            if( vertexOldDecl[iChannelIndex].Usage == D3DDECLUSAGE_NORMAL )
            {
                bHadNormal = true;
            }

            if( vertexOldDecl[iChannelIndex].Usage == D3DDECLUSAGE_TANGENT )
            {
                bHadTangent = true;
            }

            if( vertexOldDecl[iChannelIndex].Usage == D3DDECLUSAGE_BINORMAL )
            {
                bHadBinormal = true;
            }
        }
    }

    if( pTempMesh == NULL && ( bHadNormal == false || bHadTangent == false || bHadBinormal == false ) )
    {
        // クローンに失敗し、かつエフェクトで接空間が必須な場合はエラー
        return E_FAIL;
    }

    //==============================================================//
    // 足りない場合は 法線／Tangent／Binormal を生成               //
    //==============================================================//
    SAFE_RELEASE( pMesh );
    pMesh = pTempMesh;

    if( !bHadNormal )
    {
        // 法線を生成
        D3DXComputeNormals( pMesh, NULL );
    }

    DWORD* rgdwAdjacency = NULL;
    rgdwAdjacency = new DWORD[ pMesh->GetNumFaces() * 3 ];

    if( rgdwAdjacency == NULL )
    {
        return E_OUTOFMEMORY;
    }
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );

    // GPU の頂点キャッシュに合わせて最適化。
    // 三角形リストの順序を並べ替えることで、同頂点の再シェーディングを減らす。
    V( pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL ) );

    if( !bHadTangent || !bHadBinormal )
    {
        ID3DXMesh* pNewMesh;

        // 法線マッピングに必要な Tangent / Binormal を計算
        if( FAILED( D3DXComputeTangentFrameEx( pMesh, D3DDECLUSAGE_TEXCOORD, 0, D3DDECLUSAGE_TANGENT, 0,
                                               D3DDECLUSAGE_BINORMAL, 0,
                                               D3DDECLUSAGE_NORMAL, 0, 0, rgdwAdjacency, -1.01f,
                                               -0.01f, -1.01f, &pNewMesh, NULL ) ) )
        {
            return E_FAIL;
        }

        SAFE_RELEASE( pMesh );
        pMesh = pNewMesh;
    }

    SAFE_DELETE_ARRAY( rgdwAdjacency );

    *ppMesh = pMesh;

    return S_OK;

}


//--------------------------------------------------------------------------------------
// デバイスリセット後に生存しないリソース（D3DPOOL_DEFAULT）や、
// バックバッファサイズに依存するリソースを作成。
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont )
    {
        V_RETURN( g_pFont->OnResetDevice() );
    }
    if( g_pEffect )
    {
        V_RETURN( g_pEffect->OnResetDevice() );
    }

    // 多数のテキスト描画をまとめるためのスプライトを生成
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite ) );

    g_LightControl.OnD3D9ResetDevice( pBackBufferSurfaceDesc );

    // カメラの射影パラメータを設定
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;

    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    const int ciHUDLeftBorderOffset = 170;
    const int ciHUDVerticalSize = 170;
    const int ciUILeftBorderOffset = 0;
    const int ciUITopBorderOffset = 40;
    const int ciUIVerticalSize = 600;
    const int ciUIHorizontalSize = 300;

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - ciHUDLeftBorderOffset, 0 );
    g_HUD.SetSize( ciHUDLeftBorderOffset, ciHUDVerticalSize );

    g_SampleUI.SetLocation( ciUILeftBorderOffset, ciUITopBorderOffset );
    g_SampleUI.SetSize( ciUIHorizontalSize, ciUIVerticalSize );

    return S_OK;

}


//--------------------------------------------------------------------------------------
// シーンの更新。D3D API に依存せず毎フレーム呼ばれる。
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime,
                           float fElapsedTime,
                           void* pUserContext )
{
    // ユーザ入力に基づきカメラを更新
    g_Camera.FrameMove( fElapsedTime );

}


//--------------------------------------------------------------------------------------
// D3D9 デバイスでシーンを描画
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice,
                             double fTime,
                             float fElapsedTime,
                             void* pUserContext )
{
    // 設定ダイアログがアクティブなら、シーンではなくダイアログを描画
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;

    }

    HRESULT hr;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXVECTOR3 vLightDir;
    D3DXCOLOR vLightDiffuse;
    UINT iPass, cPasses;

    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProjection;

    // レンダーターゲットと Z バッファをクリア
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DXCOLOR( 0.0f, 0.25f, 0.25f, 0.55f ), 1.0f,
                          0 ) );

    // シーンをレンダリング
    if( FAILED( pd3dDevice->BeginScene() ) )
    {
        // BeginScene に失敗。これ以上できることはない。
        printf( "BeginScene() FAILED\n" );
        return;
    }

    if( g_pEffect )
    {
        // カメラから行列を取得
        mWorld = g_mWorldFix * ( *g_Camera.GetWorldMatrix() );
        mProjection = ( *g_Camera.GetProjMatrix() );
        mView = ( *g_Camera.GetViewMatrix() );

        mWorldViewProjection = mWorld * mView * mProjection;

        // カメラ位置を取得
        D3DXVECTOR4 vEye;
        D3DXVECTOR3 vTemp = ( *g_Camera.GetEyePt() );
        vEye.x = vTemp.x;
        vEye.y = vTemp.y;
        vEye.z = vTemp.z;
        vEye.w = 1.0;

        // ライト矢印を描画（ユーザがライト方向を視覚的に確認できるように）
        D3DXCOLOR arrowColor = D3DXCOLOR( 1, 1, 0, 1 );
        V( g_LightControl.OnRender9( arrowColor, &mView, &mProjection, g_Camera.GetEyePt() ) );
        vLightDir = g_LightControl.GetLightDirection();
        vLightDiffuse = g_fLightScale * D3DXCOLOR( 1, 1, 1, 1 );

        V( g_pEffect->SetValue( "g_LightDir", &vLightDir, sizeof( D3DXVECTOR3 ) ) );
        V( g_pEffect->SetValue( "g_LightDiffuse", &vLightDiffuse, sizeof( D3DXVECTOR4 ) ) );

        // エフェクトの各種定数を更新。
        // 文字列で取得するより、実際のアプリでは GetParameterByName のハンドルをキャッシュして使う方が効率的。
        V( g_pEffect->SetMatrix( "g_mWorldViewProjection", &mWorldViewProjection ) );
        V( g_pEffect->SetMatrix( "g_mWorld", &mWorld ) );
        V( g_pEffect->SetMatrix( "g_mView", &mView ) );
        V( g_pEffect->SetVector( "g_vEye", &vEye ) );
        V( g_pEffect->SetValue( "g_fHeightMapScale", &g_fHeightScale, sizeof( float ) ) );

        D3DXCOLOR vWhite = D3DXCOLOR( 1, 1, 1, 1 );
        V( g_pEffect->SetValue( "g_materialDiffuseColor", &vWhite, sizeof( D3DXCOLOR ) ) );

        V( g_pEffect->SetValue( "g_materialAmbientColor", &g_colorMtrlAmbient, sizeof( D3DXCOLOR ) ) );
        V( g_pEffect->SetValue( "g_materialDiffuseColor", &g_colorMtrlDiffuse, sizeof( D3DXCOLOR ) ) );
        V( g_pEffect->SetValue( "g_materialSpecularColor", &g_colorMtrlSpecular, sizeof( D3DXCOLOR ) ) );


        V( g_pEffect->SetValue( "g_fSpecularExponent", &g_fSpecularExponent, sizeof( float ) ) );
        V( g_pEffect->SetValue( "g_fBaseTextureRepeat", &g_fBaseTextureRepeat, sizeof( float ) ) );
        V( g_pEffect->SetValue( "g_nLODThreshold", &g_nLODThreshold, sizeof( int ) ) );
        V( g_pEffect->SetValue( "g_nMinSamples", &g_nMinSamples, sizeof( int ) ) );
        V( g_pEffect->SetValue( "g_nMaxSamples", &g_nMaxSamples, sizeof( int ) ) );
        V( g_pEffect->SetValue( "g_fShadowSoftening", &g_fShadowSoftening, sizeof( float ) ) );

        V( g_pEffect->SetBool( "g_bVisualizeLOD", ( BOOL )g_bVisualizeLOD ) );
        V( g_pEffect->SetBool( "g_bVisualizeMipLevel", ( BOOL )g_bVisualizeMipLevel ) );
        V( g_pEffect->SetBool( "g_bDisplayShadows", ( BOOL )g_bDisplayShadows ) );
        V( g_pEffect->SetBool( "g_bAddSpecular", ( BOOL )g_bAddSpecular ) );

        // シーンを描画
        switch( g_nCurrentTechniqueID )
        {
            case POM:
                V( g_pEffect->SetTechnique( "RenderSceneWithPOM" ) );
                break;

            case BUMPMAP:
                V( g_pEffect->SetTechnique( "RenderSceneWithBumpMap" ) );
                break;

            case PM:
                V( g_pEffect->SetTechnique( "RenderSceneWithPM" ) );
                break;
        }

        V( g_pEffect->Begin( &cPasses, 0 ) );
        for( iPass = 0; iPass < cPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );
            V( g_pMesh->DrawSubset( 0 ) );
            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );
    }

    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );

    RenderText( fTime );

    V( pd3dDevice->EndScene() );
}


//--------------------------------------------------------------------------------------
// ヘルプや統計情報を描画。ID3DXFont を使って効率的にテキスト描画を行う。
//--------------------------------------------------------------------------------------
void RenderText( double fTime )
{
    CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 15 );

    // 統計表示
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 2, 0 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );

    // ヘルプ表示
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 2, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls:" );

        txtHelper.SetInsertionPos( 20, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Rotate model: Left mouse button\n"
                                L"Rotate light: Right mouse button\n"
                                L"Rotate camera: Middle mouse button\n"
                                L"Zoom camera: Mouse wheel scroll\n" );

        txtHelper.SetInsertionPos( 250, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Hide help: F1\n"
                                L"Quit: ESC\n" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }
    txtHelper.End();
}


//--------------------------------------------------------------------------------------
// アプリケーションへのメッセージを処理
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd,
                          UINT uMsg,
                          WPARAM wParam,
                          LPARAM lParam,
                          bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // まずダイアログリソースマネージャへ渡して、GUI の状態更新を適切に行う
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // HUD / サンプル UI に処理させる
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    // ライトコントロールへメッセージを渡す
    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

    // 残りのメッセージはカメラへ渡してユーザ入力に応答
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// キー入力の処理
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
            {
                g_bShowHelp = !g_bShowHelp;
            }
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// ユーザ入力により POM テクスチャペアを切り替える
//--------------------------------------------------------------------------------------
void SwitchPOMTextures( DXUTComboBoxItem* pSelectedItem )
{
    SetPOMTextures( ( int )( INT_PTR )pSelectedItem->pData );
    WCHAR strLabel[s_iMAX_STRING_SIZE];

    // スライダーの範囲と値を更新
    CDXUTSlider* pHeightRangeSlider = g_SampleUI.GetSlider( IDC_HEIGHT_SCALE_SLIDER );
    if( pHeightRangeSlider )
    {
        pHeightRangeSlider->SetRange( g_iHeightScaleSliderMin, g_iHeightScaleSliderMax );
        pHeightRangeSlider->SetValue( ( int )( g_fHeightScale * g_fHeightScaleUIScale ) );

    }

    // Static 表示の更新
    CDXUTStatic* pHeightRangeStatic = g_SampleUI.GetStatic( IDC_HEIGHT_SCALE_STATIC );
    if( pHeightRangeStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Height scale: %0.2f", g_fHeightScale );
        pHeightRangeStatic->SetText( strLabel );

    }

    // スペキュラ指数スライダーの更新
    CDXUTSlider* pSpecularSlider = g_SampleUI.GetSlider( IDC_SPECULAR_EXPONENT_SLIDER );
    if( pSpecularSlider )
    {
        pSpecularSlider->SetValue( ( int )g_fSpecularExponent );

    }

    CDXUTStatic* pSpecularStatic = g_SampleUI.GetStatic( IDC_SPECULAR_EXPONENT_STATIC );
    if( pSpecularStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Specular exponent: %0.0f", g_fSpecularExponent );
        pSpecularStatic->SetText( strLabel );

    }

    // 最小サンプル数スライダーの更新
    CDXUTSlider* pMinSlider = g_SampleUI.GetSlider( IDC_MIN_NUM_SAMPLES_SLIDER );
    if( pMinSlider )
    {
        pMinSlider->SetValue( g_nMinSamples );

    }

    CDXUTStatic* pMinStatic = g_SampleUI.GetStatic( IDC_MIN_NUM_SAMPLES_STATIC );
    if( pMinStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Minium number of samples: %d", g_nMinSamples );
        pMinStatic->SetText( strLabel );

    }

    // 最大サンプル数スライダーの更新
    CDXUTSlider* pMaxSlider = g_SampleUI.GetSlider( IDC_MAX_NUM_SAMPLES_SLIDER );
    if( pMaxSlider )
    {
        pMaxSlider->SetValue( g_nMaxSamples );

    }

    CDXUTStatic* pMaxStatic = g_SampleUI.GetStatic( IDC_MAX_NUM_SAMPLES_STATIC );
    if( pMaxStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Maximum number of samples: %d", g_nMaxSamples );
        pMaxStatic->SetText( strLabel );
    }
}


//--------------------------------------------------------------------------------------
// GUI イベントの処理
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
        {
            DXUTToggleFullScreen();
            break;
        }

        case IDC_TOGGLEREF:
        {
            DXUTToggleREF();
            break;
        }

        case IDC_CHANGEDEVICE:
        {
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
        }

        case IDC_SPECULAR_EXPONENT_SLIDER:
        {
            g_fSpecularExponent = ( float )( g_SampleUI.GetSlider( IDC_SPECULAR_EXPONENT_SLIDER )->GetValue() );
            WCHAR sz[s_iMAX_STRING_SIZE];
            swprintf_s( sz, s_iMAX_STRING_SIZE, L"Specular exponent: %0.0f", g_fSpecularExponent );
            g_SampleUI.GetStatic( IDC_SPECULAR_EXPONENT_STATIC )->SetText( sz );
            break;
        }

        case IDC_HEIGHT_SCALE_SLIDER:
        {
            g_fHeightScale = ( float )( g_SampleUI.GetSlider( IDC_HEIGHT_SCALE_SLIDER )->GetValue() /
                                        g_fHeightScaleUIScale );
            WCHAR sz[s_iMAX_STRING_SIZE];
            swprintf_s( sz, s_iMAX_STRING_SIZE, L"Height scale: %0.2f", g_fHeightScale );
            g_SampleUI.GetStatic( IDC_HEIGHT_SCALE_STATIC )->SetText( sz );
            break;
        }

        case IDC_MIN_NUM_SAMPLES_SLIDER:
        {
            g_nMinSamples = g_SampleUI.GetSlider( IDC_MIN_NUM_SAMPLES_SLIDER )->GetValue();
            WCHAR strLabel[s_iMAX_STRING_SIZE];
            swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Minimum number of samples: %d", g_nMinSamples );
            g_SampleUI.GetStatic( IDC_MIN_NUM_SAMPLES_STATIC )->SetText( strLabel );
            break;
        }

        case IDC_MAX_NUM_SAMPLES_SLIDER:
        {
            g_nMaxSamples = g_SampleUI.GetSlider( IDC_MAX_NUM_SAMPLES_SLIDER )->GetValue();

            WCHAR strLabel[s_iMAX_STRING_SIZE];
            swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Maxinum number of samples: %d", g_nMaxSamples );
            g_SampleUI.GetStatic( IDC_MAX_NUM_SAMPLES_STATIC )->SetText( strLabel );
            break;
        }

        case IDC_TOGGLE_SHADOWS:
        {
            // 影の ON/OFF
            g_bDisplayShadows = ( bool )( ( CDXUTCheckBox* )pControl )->GetChecked();

            WCHAR strLabel[s_iMAX_STRING_SIZE];
            if( g_bDisplayShadows )
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle self-occlusion shadows rendering: ON " );
            else
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle self-occlusion shadows rendering: OFF " );
            g_SampleUI.GetCheckBox( IDC_TOGGLE_SHADOWS )->SetText( strLabel );
            break;
        }

        case IDC_TECHNIQUE_COMBO_BOX:
        {
            DXUTComboBoxItem* pSelectedItem = ( ( CDXUTComboBox* )pControl )->GetSelectedItem();
            if( pSelectedItem )
                g_nCurrentTechniqueID = ( int )( INT_PTR )pSelectedItem->pData;

            break;
        }

        case IDC_TOGGLE_SPECULAR:
        {
            // スペキュラの ON/OFF
            g_bAddSpecular = ( bool )( ( CDXUTCheckBox* )pControl )->GetChecked();

            WCHAR strLabel[s_iMAX_STRING_SIZE];
            if( g_bAddSpecular )
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle specular: ON" );
            else
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle specular: OFF" );
            g_SampleUI.GetCheckBox( IDC_TOGGLE_SPECULAR )->SetText( strLabel );
            break;
        }

        case IDC_SELECT_TEXTURES_COMBOBOX:
        {
            DXUTComboBoxItem* pSelectedItem = ( ( CDXUTComboBox* )pControl )->GetSelectedItem();
            if( pSelectedItem )
                SwitchPOMTextures( pSelectedItem );
            break;
        }

        case IDC_RELOAD_BUTTON:
        {
            HRESULT hr;
            V( LoadEffectFile() );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// OnD3D9ResetDevice で作成した D3D9 リソースを解放
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    CDXUTDirectionWidget::StaticOnD3D9LostDevice();

    if( g_pFont ) g_pFont->OnLostDevice();
    if( g_pEffect ) g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pSprite );
}


//--------------------------------------------------------------------------------------
// OnD3D9CreateDevice で作成した D3D9 リソースを解放
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D9DestroyDevice();

    // メッシュ／フォント／エフェクト／テクスチャを解放
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pMesh );

    for( int iTextureIndex = POM_STONES; iTextureIndex < s_iNUM_TEXTURES; iTextureIndex++ )
    {
        SAFE_RELEASE( g_pBaseTextures[iTextureIndex] );
        SAFE_RELEASE( g_pNMHTextures[iTextureIndex] );
    }

    free( g_pBaseTextures );
    free( g_pNMHTextures );
}


#if 0
//=================================================================================================
// File: ParallaxOcclusionMapping.cpp
//
// Author: Natalya Tatarchuk
//         ATI Research, Inc.
//         3D Application Research Group  
//
// Implementation of the algorithm as described in "Dynamic Parallax Occlusion Mapping with 
// Approximate Soft Shadows" paper, by N. Tatarchuk, ATI Research, to appear in the proceedings 
// of ACM Symposium on Interactive 3D Graphics and Games, 2006.
//                 
// Copyright (c) ATI Research, Inc. All rights reserved.
//=================================================================================================

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 


//--------------------------------------------------------------------------------------
// Global variables and parameters
//--------------------------------------------------------------------------------------
const int                   s_iMAX_STRING_SIZE = 100;
const int                   s_iDECL_SIZE = 56;
const int                   s_iNUM_TEXTURES = 7;  // Number of loaded texturs in the program

//--------------------------------------------------------------------------------------
// Texture pair ids for use for this sample
//--------------------------------------------------------------------------------------
enum POM_TEXTURE_IDS
{
    POM_STONES = 0,
    POM_ROCKS,
    POM_WALL,
    POM_FOUR_BUMPS,
    POM_BUMPS,
    POM_DENTS,
    POM_SAINT
};

//--------------------------------------------------------------------------------------
// Id numbers for different rendering techniques used
//--------------------------------------------------------------------------------------
enum TECHNIQUE_IDS
{
    POM,           // Parallax occlusion mapping
    BUMPMAP,       // Bump mapping
    PM             // Parallax mapping with offset limiting
};

int                         g_nCurrentTextureID = POM_STONES;
int                         g_nCurrentTechniqueID = POM;

//--------------------------------------------------------------------------------------
// Texture pair names for use for this sample
//--------------------------------------------------------------------------------------
WCHAR*                      g_strBaseTextureNames[] =
{
    TEXT( "stones.bmp" ),
    TEXT( "rocks.jpg" ),
    TEXT( "wall.jpg" ),
    TEXT( "wood.jpg" ),
    TEXT( "concrete.bmp" ),
    TEXT( "concrete.bmp" ),
    TEXT( "wood.jpg" )
};

WCHAR*                      g_strNMHTextureNames[] =
{
    TEXT( "stones_NM_height.tga" ),
    TEXT( "rocks_NM_height.tga" ),
    TEXT( "wall_NM_height.tga" ),
    TEXT( "four_NM_height.tga" ),
    TEXT( "bump_NM_height.tga" ),
    TEXT( "dent_NM_height.tga" ),
    TEXT( "saint_NM_height.tga" )

};


//--------------------------------------------------------------------------------------
IDirect3DDevice9*           g_pD3DDevice = NULL;       // A pointer to the current D3D device used for rendering

ID3DXFont*                  g_pFont = NULL;        // Font for drawing text
ID3DXSprite*                g_pSprite = NULL;        // Sprite for batching draw text calls
bool                        g_bShowHelp = false;       // If true, it renders the UI control text
CModelViewerCamera          g_Camera;                  // A model viewing camera
ID3DXEffect*                g_pEffect = NULL;        // D3DX effect interface

ID3DXMesh*                  g_pMesh = NULL;            // Mesh object

IDirect3DTexture9**         g_pBaseTextures = NULL;    // Array of base map texture surfaces
IDirect3DTexture9**         g_pNMHTextures = NULL;    // Array of normal / height map texture surfaces
//       a height map in the alpha channel

CDXUTDialogResourceManager  g_DialogResourceManager;   // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;             // Device settings dialog
CDXUTDialog                 g_HUD;                     // manages the 3D UI
CDXUTDialog                 g_SampleUI;                // dialog for sample specific controls
D3DXMATRIXA16               g_mWorldFix;

CDXUTDirectionWidget        g_LightControl;            // Scene light
float                       g_fLightScale;             // Light brightness scale parameter


//--------------------------------------------------------------------------------------
// UI scalar parameters
//--------------------------------------------------------------------------------------
const float                 s_fLightScaleUIScale = 10.0f;
const int                   s_iLightScaleSliderMin = 0;
const int                   s_iLightScaleSliderMax = 20;

int                         g_iHeightScaleSliderMin = 0;
int                         g_iHeightScaleSliderMax = 100;
float                       g_fHeightScaleUIScale = 100.0f;

const int                   s_iSliderMin = 8;
const int                   s_iSliderMax = 130;

//--------------------------------------------------------------------------------------
// Material properties parameters:
//--------------------------------------------------------------------------------------
D3DXCOLOR                   g_colorMtrlDiffuse( 1.0f, 1.0f, 1.0f, 1.0f );
D3DXCOLOR                   g_colorMtrlAmbient( 0.35f, 0.35f, 0.35f, 0 );
D3DXCOLOR                   g_colorMtrlSpecular( 1.0f, 1.0f, 1.0f, 1.0f );

float                       g_fSpecularExponent( 60.0f );        // Material's specular exponent
float                       g_fBaseTextureRepeat( 1.0f );        // The tiling factor for base and normal map textures

bool                        g_bVisualizeLOD( false );            // Toggles visualization of level of detail colors
bool                        g_bVisualizeMipLevel( false );       // Toggles visualization of mip level
bool                        g_bDisplayShadows( true );           // Toggles display of self-occlusion based shadows
bool                        g_bAddSpecular( true );              // Toggles rendering with specular or without
bool                        g_bRenderPOM( true );                // Toggles whether using normal mapping or parallax occlusion mapping

int                         g_nLODThreshold( 3 );                // The mip level id for transitioning between the full computation
// for parallax occlusion mapping and the bump mapping computation
int                         g_nMinSamples( 8 );                  // The minimum number of samples for sampling the height field profile
int                         g_nMaxSamples( 50 );                 // The maximum number of samples for sampling the height field profile
float                       g_fShadowSoftening( 0.58f );         // Blurring factor for the soft shadows computation
float                       g_fHeightScale;                    // This parameter controls the height map range for displacement mapping

//--------------------------------------------------------------------------------------
// Mesh file names for use
//--------------------------------------------------------------------------------------
WCHAR*                      g_strMeshFileName0 = TEXT( "Disc.x" );


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN           1
#define IDC_TOGGLEREF                  3
#define IDC_CHANGEDEVICE               4
#define IDC_HEIGHT_SCALE_SLIDER        5
#define IDC_HEIGHT_SCALE_STATIC        6
#define IDC_TOGGLE_SHADOWS             7
#define IDC_SELECT_TEXTURES_COMBOBOX   8
#define IDC_TOGGLE_SPECULAR            9
#define IDC_SPECULAR_EXPONENT_SLIDER   10
#define IDC_SPECULAR_EXPONENT_STATIC   11
#define IDC_MIN_NUM_SAMPLES_SLIDER     12
#define IDC_MIN_NUM_SAMPLES_STATIC     13
#define IDC_MAX_NUM_SAMPLES_SLIDER     14
#define IDC_MAX_NUM_SAMPLES_STATIC     15
#define IDC_TECHNIQUE_COMBO_BOX        16
#define IDC_RELOAD_BUTTON              20


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed,
                                  void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnLostDevice( void* pUserContext );
void CALLBACK OnDestroyDevice( void* pUserContext );

void InitApp();
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh );
void RenderText( double fTime );


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a 
// message processing loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI wWinMain( HINSTANCE, HINSTANCE, LPWSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
      _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
   #endif

    // Set the callback functions
    DXUTSetCallbackD3D9DeviceAcceptable( IsDeviceAcceptable );
    DXUTSetCallbackD3D9DeviceCreated( OnCreateDevice );
    DXUTSetCallbackD3D9DeviceReset( OnResetDevice );
    DXUTSetCallbackD3D9FrameRender( OnFrameRender );
    DXUTSetCallbackD3D9DeviceLost( OnLostDevice );
    DXUTSetCallbackD3D9DeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCursorSettings( true, true );
    InitApp();
    DXUTInit( true, true ); // Parse the command line and show msgboxes
    DXUTSetHotkeyHandling( true, true, true );  // handle the defaul hotkeys
    DXUTCreateWindow( L"Parallax Occlusion Mapping" );
    DXUTCreateDevice( true, 800, 600 );
    DXUTMainLoop();

    return DXUTGetExitCode();

}

//--------------------------------------------------------------------------------------
// Select a pair of base and normal map / height map textures to use and
// setup related height map range parameters, given a texture index
// Note: all texture surfaces in g_pBaseTextures and g_pNMHTextures MUST
// be created prior to calling this function.
//--------------------------------------------------------------------------------------
void SetPOMTextures( int iTextureIndex )
{
    g_nCurrentTextureID = iTextureIndex;

    // Bind the new active textures to the correct texture slots in the shaders:
    if( g_pD3DDevice && g_pEffect )
    {
        HRESULT hr;

        V( g_pEffect->SetTexture( "g_baseTexture", g_pBaseTextures[g_nCurrentTextureID] ) );
        V( g_pEffect->SetTexture( "g_nmhTexture", g_pNMHTextures[g_nCurrentTextureID] ) );

    }

    // Setup height map range slider parameters (need to be setup per-texture, as very height-map specific:
    switch( iTextureIndex )
    {
        case POM_STONES:
        {
            // Stones texture pair:
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.02f;

            g_fSpecularExponent = 60.0f;
            g_fBaseTextureRepeat = 1.0f;

            g_nMinSamples = 8;
            g_nMaxSamples = 50;
            break;
        }

        case POM_ROCKS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.1f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 1.0f;

            g_nMinSamples = 8;
            g_nMaxSamples = 100;
            break;
        }

        case POM_WALL:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.06f;

            g_fSpecularExponent = 60.0f;
            g_fBaseTextureRepeat = 1.0f;

            g_nMinSamples = 8;
            g_nMaxSamples = 50;
            break;
        }

        case POM_FOUR_BUMPS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 10.0f;
            g_fHeightScale = 0.2f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 1.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }

        case POM_BUMPS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 10.0f;
            g_fHeightScale = 0.2f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 4.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }

        case POM_DENTS:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 10.0f;
            g_fHeightScale = 0.2f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 4.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }

        case POM_SAINT:
        {
            g_iHeightScaleSliderMin = 0;
            g_iHeightScaleSliderMax = 10;
            g_fHeightScaleUIScale = 100.0f;
            g_fHeightScale = 0.08f;

            g_fSpecularExponent = 100.0f;
            g_fBaseTextureRepeat = 1.0f;
            g_nMinSamples = 12;
            g_nMaxSamples = 100;
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Initialize the app: initialize the light controls and the GUI 
// elements for this application.
//--------------------------------------------------------------------------------------
void InitApp()
{
    // UI elements parameters:
    const int ciPadding = 24;        // vertical padding between controls
    const int ciLeft = 5;         // Left align border (x-coordinate) of the controls
    const int ciWidth = 125;       // widget width
    const int ciHeight = 22;        // widget height

    // Initialize the light control:
    g_LightControl.SetLightDirection( D3DXVECTOR3( sinf( D3DX_PI * 2 - D3DX_PI / 6 ),
                                                   0,
                                                   -cosf( D3DX_PI * 2 - D3DX_PI / 6 ) ) );
    g_fLightScale = 1.0f;

    // Initialize POM textures and height map parameters:
    SetPOMTextures( POM_STONES );

    // Initialize dialogs // 
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    // Initialize user interface elements //

    // Top toggle buttons //
    g_HUD.SetCallback( OnGUIEvent );

    int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", ciLeft, iY, ciWidth, ciHeight );
    iY += ciPadding;
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", ciLeft, iY, ciWidth, ciHeight );
    iY += ciPadding;
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", ciLeft, iY, ciWidth, ciHeight, VK_F2 );
    iY += ciPadding;

    // Reload button only works in debug:
#ifdef _DEBUG
      g_HUD.AddButton( IDC_RELOAD_BUTTON, L"Reload effect", ciLeft, iY, ciWidth, ciHeight );
   #endif

    iY = 10;
    g_SampleUI.SetCallback( OnGUIEvent );

    WCHAR sz[s_iMAX_STRING_SIZE];

    // Slider parameters:
    const int ciSliderLeft = 15;
    const int ciSliderWidth = 100;

    // Technique selection combo box
    CDXUTComboBox* pTechniqueComboBox = NULL;
    g_SampleUI.AddComboBox( IDC_TECHNIQUE_COMBO_BOX, ciLeft, iY, 200, 24, L'O', false, &pTechniqueComboBox );
    if( pTechniqueComboBox )
    {
        pTechniqueComboBox->SetDropHeight( 100 );
        pTechniqueComboBox->AddItem( L"Parallax Occlusion Mapping", ( LPVOID )POM );
        pTechniqueComboBox->AddItem( L"Bump Mapping", ( LPVOID )BUMPMAP );
        pTechniqueComboBox->AddItem( L"Parallax Mapping with Offset Limiting", ( LPVOID )PM );

    }

    // Add control for height range //
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Height scale: %0.2f", g_fHeightScale );
    g_SampleUI.AddStatic( IDC_HEIGHT_SCALE_STATIC, sz, ciLeft, iY, ciWidth, ciHeight );

    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_HEIGHT_SCALE_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight, g_iHeightScaleSliderMin,
                          g_iHeightScaleSliderMax,
                          ( int )( g_fHeightScale * g_fHeightScaleUIScale ) );

    // Texture selection combo box:
    iY += ciPadding;
    CDXUTComboBox* pTextureComboBox = NULL;
    g_SampleUI.AddComboBox( IDC_SELECT_TEXTURES_COMBOBOX, ciLeft, iY, 200, 24, L'O', false, &pTextureComboBox );
    if( pTextureComboBox )
    {
        pTextureComboBox->SetDropHeight( 100 );
        pTextureComboBox->AddItem( L"Stones", ( LPVOID )POM_STONES );
        pTextureComboBox->AddItem( L"Rocks", ( LPVOID )POM_ROCKS );
        pTextureComboBox->AddItem( L"Wall", ( LPVOID )POM_WALL );
        pTextureComboBox->AddItem( L"Sphere, Triangle, Torus, Pyramid", ( LPVOID )POM_FOUR_BUMPS );
        pTextureComboBox->AddItem( L"Bumps", ( LPVOID )POM_BUMPS );
        pTextureComboBox->AddItem( L"Dents", ( LPVOID )POM_DENTS );
        pTextureComboBox->AddItem( L"Relief", ( LPVOID )POM_SAINT );

    }

    // Toggle shadows checkbox
    iY += ciPadding;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_SHADOWS, L"Toggle self-occlusion shadows rendering: ON",
                            ciLeft, iY, 350, 24, g_bDisplayShadows, L'C', false );

    // Toggle specular checkbox
    iY += ciPadding;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_SPECULAR, L"Toggle specular: ON",
                            ciLeft, iY, 350, 24, g_bAddSpecular, L'C', false );


    // Specular exponent slider:
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Specular exponent: %0.0f", g_fSpecularExponent );
    g_SampleUI.AddStatic( IDC_SPECULAR_EXPONENT_STATIC, sz, ciLeft, iY, ciWidth, ciHeight );

    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_SPECULAR_EXPONENT_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight,
                          s_iSliderMin, s_iSliderMax, ( int )g_fSpecularExponent );

    // Number of samples: minimum
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Minium number of samples: %d", g_nMinSamples );
    g_SampleUI.AddStatic( IDC_MIN_NUM_SAMPLES_STATIC, sz, ciLeft, iY, ciWidth * 2, ciHeight );
    g_SampleUI.GetStatic( IDC_MIN_NUM_SAMPLES_STATIC )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP |
        DT_WORDBREAK;

    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_MIN_NUM_SAMPLES_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight,
                          s_iSliderMin, s_iSliderMax, g_nMinSamples );

    // Number of samples: maximum
    iY += ciPadding;
    swprintf_s( sz, s_iMAX_STRING_SIZE, L"Maximum number of samples: %d", g_nMaxSamples );
    g_SampleUI.AddStatic( IDC_MAX_NUM_SAMPLES_STATIC, sz, ciLeft, iY, ciWidth * 2, ciHeight );
    g_SampleUI.GetStatic( IDC_MAX_NUM_SAMPLES_STATIC )->GetElement( 0 )->dwTextFormat = DT_LEFT | DT_TOP |
        DT_WORDBREAK;


    iY += ciPadding;
    g_SampleUI.AddSlider( IDC_MAX_NUM_SAMPLES_SLIDER, ciSliderLeft, iY, ciSliderWidth, ciHeight,
                          s_iSliderMin, s_iSliderMax, g_nMaxSamples );

}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by 
// returning E_FAIL.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps,
                                  D3DFORMAT AdapterFormat,
                                  D3DFORMAT BackBufferFormat,
                                  bool bWindowed,
                                  void* pUserContext )
{
    // The ParallaxOcculsionMapping technique requires PS 3.0 so reject any device that 
    // doesn't support at least PS 3.0.  Typically, the app would fallback but 
    // ParallaxOcculsionMapping is the purpose of this sample
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 3, 0 ) )
    {
        return false;
    }

    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal,
                                         pCaps->DeviceType,
                                         AdapterFormat,
                                         D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
                                         D3DRTYPE_TEXTURE,
                                         BackBufferFormat ) ) )
    {
        return false;
    }

    return true;

}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings,
                                    void* pUserContext )
{
    assert( DXUT_D3D9_DEVICE == pDeviceSettings->ver );

    HRESULT hr;
    IDirect3D9* pD3D = DXUTGetD3D9Object();
    D3DCAPS9 caps;

    V( pD3D->GetDeviceCaps( pDeviceSettings->d3d9.AdapterOrdinal,
                            pDeviceSettings->d3d9.DeviceType,
                            &caps ) );

    // If device doesn't support VS 3.0 then switch to SWVP
    if( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF &&
        ( ( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) == 0 || caps.VertexShaderVersion < D3DVS_VERSION( 3,
                                                                                                              0 ) ) )
    {
        pDeviceSettings->d3d9.BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // Debugging vertex shaders requires either REF or software vertex processing 
    // and debugging pixel shaders requires REF.  
#ifdef DEBUG_VS
      if ( pDeviceSettings->d3d9.DeviceType != D3DDEVTYPE_REF )
      {
         pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
         pDeviceSettings->d3d9.BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
         pDeviceSettings->d3d9.BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
      }
   #endif
#ifdef DEBUG_PS
      pDeviceSettings->d3d9.DeviceType = D3DDEVTYPE_REF;
   #endif

    // Turn off VSynch
    pDeviceSettings->d3d9.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Loads the effect file from disk. Note: D3D device must be created and set 
// prior to calling this method.
//--------------------------------------------------------------------------------------
HRESULT LoadEffectFile()
{
    SAFE_RELEASE( g_pEffect );

    if( g_pD3DDevice == NULL )
        return E_FAIL;

    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;

#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DXSHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DXSHADER_DEBUG;
    #endif

#ifdef DEBUG_VS
      dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
   #endif
#ifdef DEBUG_PS
      dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
   #endif

    WCHAR str[MAX_PATH];
    HRESULT hr;
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ParallaxOcclusionMapping.fx" ) );

    ID3DXBuffer* pErrors = NULL;
    hr = D3DXCreateBuffer( 1024, &pErrors );

    hr = D3DXCreateEffectFromFile( g_pD3DDevice, str, NULL, NULL, dwShaderFlags, NULL, &g_pEffect, &pErrors );

    if( FAILED( hr ) )
    {
        // Output shader compilation errors to the shell:
        CHAR* pErrorStr = ( CHAR* )pErrors->GetBufferPointer();
        printf( "%s\n", pErrorStr );

        return E_FAIL;
    }

    SAFE_RELEASE( pErrors );

    return S_OK;

}

//--------------------------------------------------------------------------------------
// Create any D3D9 resources that will live through a device reset (D3DPOOL_MANAGED)
// and aren't tied to the back buffer size
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice,
                                 const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                 void* pUserContext )
{
    g_pD3DDevice = pd3dDevice;

    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9CreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnD3D9CreateDevice( pd3dDevice ) );

    // Initialize the font 
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont ) );

    // Load the mesh 
    V_RETURN( LoadMesh( pd3dDevice, g_strMeshFileName0, &g_pMesh ) );

    D3DXVECTOR3* pData;
    D3DXVECTOR3 vCenter;
    FLOAT fObjectRadius;
    V( g_pMesh->LockVertexBuffer( 0, ( LPVOID* )&pData ) );
    V( D3DXComputeBoundingSphere( pData, g_pMesh->GetNumVertices(), s_iDECL_SIZE, &vCenter, &fObjectRadius ) );
    V( g_pMesh->UnlockVertexBuffer() );

    // Align the starting frame of the mesh to be slightly toward the viewer yet to 
    // see the grazing angles:                                                      
    D3DXMatrixTranslation( &g_mWorldFix, -vCenter.x, -vCenter.y, -vCenter.z );
    D3DXMATRIXA16 mRotation;
    D3DXMatrixRotationY( &mRotation, -D3DX_PI / 3.0f );
    g_mWorldFix *= mRotation;
    D3DXMatrixRotationX( &mRotation, D3DX_PI / 3.0f );
    g_mWorldFix *= mRotation;

    // Initialize the light control 
    V_RETURN( CDXUTDirectionWidget::StaticOnD3D9CreateDevice( pd3dDevice ) );
    g_LightControl.SetRadius( fObjectRadius );

    V_RETURN( LoadEffectFile() );

    // Load all textures used by the program from disk 
    WCHAR str[MAX_PATH];

    g_pBaseTextures = ( IDirect3DTexture9** )malloc( sizeof( IDirect3DTexture9* ) * s_iNUM_TEXTURES );
    if( g_pBaseTextures == NULL )
    {
        // ERROR allocating the array for base texture pointers storage!
        printf( "ERROR allocating the array for base texture pointers storage!\n" );
        return E_FAIL;
    }
    g_pNMHTextures = ( IDirect3DTexture9** )malloc( sizeof( IDirect3DTexture9* ) * s_iNUM_TEXTURES );
    if( g_pNMHTextures == NULL )
    {
        // ERROR allocating the array for normal map / height map texture pointers storage!
        printf( "ERROR allocating the array for normal map / height map texture pointers storage!\n" );
        return E_FAIL;
    }

    for( int iTextureIndex = POM_STONES; iTextureIndex < s_iNUM_TEXTURES; iTextureIndex++ )
    {
        // Load the pair of textures (base and combined normal map / height map texture) into 
        // memory for each type of POM texture pairs

        // Load the base mesh:
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_strBaseTextureNames[iTextureIndex] ) );

        V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str, D3DX_DEFAULT, D3DX_DEFAULT,
                                               D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                               D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                               NULL, NULL, &g_pBaseTextures[iTextureIndex] ) );

        // Load the normal map / height map texture
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_strNMHTextureNames[iTextureIndex] ) );

        V_RETURN( D3DXCreateTextureFromFileEx( pd3dDevice, str, D3DX_DEFAULT, D3DX_DEFAULT,
                                               D3DX_DEFAULT, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
                                               D3DX_DEFAULT, D3DX_DEFAULT, 0,
                                               NULL, NULL, &g_pNMHTextures[iTextureIndex] ) );
    }

    SetPOMTextures( g_nCurrentTextureID );

    // Setup the camera's view parameters 
    D3DXVECTOR3 vecEye( 0.0f, 0.0f, -15.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 0.5f, fObjectRadius * 10.0f );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// This function loads the mesh and ensures the mesh has normals; it also 
// optimizes the mesh for the graphics card's vertex cache, which improves 
// performance by organizing the internal triangle list for less cache misses.
//--------------------------------------------------------------------------------------
HRESULT LoadMesh( IDirect3DDevice9* pd3dDevice, WCHAR* strFileName, ID3DXMesh** ppMesh )
{
    ID3DXMesh* pMesh = NULL;
    WCHAR str[MAX_PATH];
    HRESULT hr;

    //====================================================================//
    // Load the mesh with D3DX and get back a ID3DXMesh*.  For this       //
    // sample we'll ignore the X file's embedded materials since we know  //
    // exactly the model we're loading.  See the mesh samples such as     //
    // "OptimizedMesh" for a more generic mesh loading example.           //
    //====================================================================//
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strFileName ) );
    V_RETURN( D3DXLoadMeshFromX( str, D3DXMESH_MANAGED, pd3dDevice, NULL, NULL, NULL, NULL, &pMesh ) );

    // Create a new vertex declaration to hold all the required data
    const D3DVERTEXELEMENT9 vertexDecl[] =
    {
        { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT,  0 },
        { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
        D3DDECL_END()
    };

    LPD3DXMESH pTempMesh = NULL;

    // Clone mesh to match the specified declaration: 
    if( FAILED( pMesh->CloneMesh( pMesh->GetOptions(), vertexDecl, pd3dDevice, &pTempMesh ) ) )
    {
        SAFE_RELEASE( pTempMesh );
        return E_FAIL;
    }

    //====================================================================//
    // Check if the old declaration contains normals, tangents, binormals //
    //====================================================================//
    bool bHadNormal = false;
    bool bHadTangent = false;
    bool bHadBinormal = false;

    D3DVERTEXELEMENT9 vertexOldDecl[ MAX_FVF_DECL_SIZE ];

    if( pMesh && SUCCEEDED( pMesh->GetDeclaration( vertexOldDecl ) ) )
    {
        // Go through the declaration and look for the right channels, hoping for a match:
        for( UINT iChannelIndex = 0; iChannelIndex < D3DXGetDeclLength( vertexOldDecl ); iChannelIndex++ )
        {
            if( vertexOldDecl[iChannelIndex].Usage == D3DDECLUSAGE_NORMAL )
            {
                bHadNormal = true;
            }

            if( vertexOldDecl[iChannelIndex].Usage == D3DDECLUSAGE_TANGENT )
            {
                bHadTangent = true;
            }

            if( vertexOldDecl[iChannelIndex].Usage == D3DDECLUSAGE_BINORMAL )
            {
                bHadBinormal = true;
            }
        }
    }

    if( pTempMesh == NULL && ( bHadNormal == false || bHadTangent == false || bHadBinormal == false ) )
    {
        // We failed to clone the mesh and we need the tangent space for our effect:
        return E_FAIL;
    }

    //==============================================================//
    // Generate normals / tangents / binormals if they were missing //
    //==============================================================//
    SAFE_RELEASE( pMesh );
    pMesh = pTempMesh;

    if( !bHadNormal )
    {
        // Compute normals in case the meshes have them
        D3DXComputeNormals( pMesh, NULL );
    }

    DWORD* rgdwAdjacency = NULL;
    rgdwAdjacency = new DWORD[ pMesh->GetNumFaces() * 3 ];

    if( rgdwAdjacency == NULL )
    {
        return E_OUTOFMEMORY;
    }
    V( pMesh->GenerateAdjacency( 1e-6f, rgdwAdjacency ) );

    // Optimize the mesh for this graphics card's vertex cache 
    // so when rendering the mesh's triangle list the vertices will 
    // cache hit more often so it won't have to re-execute the vertex shader 
    // on those vertices so it will improve perf.     
    V( pMesh->OptimizeInplace( D3DXMESHOPT_VERTEXCACHE, rgdwAdjacency, NULL, NULL, NULL ) );

    if( !bHadTangent || !bHadBinormal )
    {
        ID3DXMesh* pNewMesh;

        // Compute tangents, which are required for normal mapping
        if( FAILED( D3DXComputeTangentFrameEx( pMesh, D3DDECLUSAGE_TEXCOORD, 0, D3DDECLUSAGE_TANGENT, 0,
                                               D3DDECLUSAGE_BINORMAL, 0,
                                               D3DDECLUSAGE_NORMAL, 0, 0, rgdwAdjacency, -1.01f,
                                               -0.01f, -1.01f, &pNewMesh, NULL ) ) )
        {
            return E_FAIL;
        }

        SAFE_RELEASE( pMesh );
        pMesh = pNewMesh;
    }

    SAFE_DELETE_ARRAY( rgdwAdjacency );

    *ppMesh = pMesh;

    return S_OK;

}


//--------------------------------------------------------------------------------------
// Create any D3D9 resources that won't live through a device reset (D3DPOOL_DEFAULT) 
// or that are tied to the back buffer size 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice,
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc,
                                void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D9ResetDevice() );
    V_RETURN( g_SettingsDlg.OnD3D9ResetDevice() );

    if( g_pFont )
    {
        V_RETURN( g_pFont->OnResetDevice() );
    }
    if( g_pEffect )
    {
        V_RETURN( g_pEffect->OnResetDevice() );
    }

    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pSprite ) );

    g_LightControl.OnD3D9ResetDevice( pBackBufferSurfaceDesc );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;

    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    const int ciHUDLeftBorderOffset = 170;
    const int ciHUDVerticalSize = 170;
    const int ciUILeftBorderOffset = 0;
    const int ciUITopBorderOffset = 40;
    const int ciUIVerticalSize = 600;
    const int ciUIHorizontalSize = 300;

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - ciHUDLeftBorderOffset, 0 );
    g_HUD.SetSize( ciHUDLeftBorderOffset, ciHUDVerticalSize );

    g_SampleUI.SetLocation( ciUILeftBorderOffset, ciUITopBorderOffset );
    g_SampleUI.SetSize( ciUIHorizontalSize, ciUIVerticalSize );

    return S_OK;

}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime,
                           float fElapsedTime,
                           void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D9 device
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice,
                             double fTime,
                             float fElapsedTime,
                             void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;

    }

    HRESULT hr;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXVECTOR3 vLightDir;
    D3DXCOLOR vLightDiffuse;
    UINT iPass, cPasses;

    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProjection;

    // Clear the render target and the zbuffer 
    V( pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DXCOLOR( 0.0f, 0.25f, 0.25f, 0.55f ), 1.0f,
                          0 ) );

    // Render the scene
    if( FAILED( pd3dDevice->BeginScene() ) )
    {
        // Failed to being rendering scene, not much we can do not:
        printf( "BeginScene() FAILED\n" );
        return;
    }

    if( g_pEffect )
    {
        // Get the projection & view matrix from the camera class
        mWorld = g_mWorldFix * ( *g_Camera.GetWorldMatrix() );
        mProjection = ( *g_Camera.GetProjMatrix() );
        mView = ( *g_Camera.GetViewMatrix() );

        mWorldViewProjection = mWorld * mView * mProjection;

        // Get camera position:
        D3DXVECTOR4 vEye;
        D3DXVECTOR3 vTemp = ( *g_Camera.GetEyePt() );
        vEye.x = vTemp.x;
        vEye.y = vTemp.y;
        vEye.z = vTemp.z;
        vEye.w = 1.0;

        // Render the light arrow so the user can visually see the light dir
        D3DXCOLOR arrowColor = D3DXCOLOR( 1, 1, 0, 1 );
        V( g_LightControl.OnRender9( arrowColor, &mView, &mProjection, g_Camera.GetEyePt() ) );
        vLightDir = g_LightControl.GetLightDirection();
        vLightDiffuse = g_fLightScale * D3DXCOLOR( 1, 1, 1, 1 );

        V( g_pEffect->SetValue( "g_LightDir", &vLightDir, sizeof( D3DXVECTOR3 ) ) );
        V( g_pEffect->SetValue( "g_LightDiffuse", &vLightDiffuse, sizeof( D3DXVECTOR4 ) ) );

        // Update the effect's variables.  Instead of using strings, it would 
        // be more efficient to cache a handle to the parameter by calling 
        // ID3DXEffect::GetParameterByName
        V( g_pEffect->SetMatrix( "g_mWorldViewProjection", &mWorldViewProjection ) );
        V( g_pEffect->SetMatrix( "g_mWorld", &mWorld ) );
        V( g_pEffect->SetMatrix( "g_mView", &mView ) );
        V( g_pEffect->SetVector( "g_vEye", &vEye ) );
        V( g_pEffect->SetValue( "g_fHeightMapScale", &g_fHeightScale, sizeof( float ) ) );

        D3DXCOLOR vWhite = D3DXCOLOR( 1, 1, 1, 1 );
        V( g_pEffect->SetValue( "g_materialDiffuseColor", &vWhite, sizeof( D3DXCOLOR ) ) );

        V( g_pEffect->SetValue( "g_materialAmbientColor", &g_colorMtrlAmbient, sizeof( D3DXCOLOR ) ) );
        V( g_pEffect->SetValue( "g_materialDiffuseColor", &g_colorMtrlDiffuse, sizeof( D3DXCOLOR ) ) );
        V( g_pEffect->SetValue( "g_materialSpecularColor", &g_colorMtrlSpecular, sizeof( D3DXCOLOR ) ) );


        V( g_pEffect->SetValue( "g_fSpecularExponent", &g_fSpecularExponent, sizeof( float ) ) );
        V( g_pEffect->SetValue( "g_fBaseTextureRepeat", &g_fBaseTextureRepeat, sizeof( float ) ) );
        V( g_pEffect->SetValue( "g_nLODThreshold", &g_nLODThreshold, sizeof( int ) ) );
        V( g_pEffect->SetValue( "g_nMinSamples", &g_nMinSamples, sizeof( int ) ) );
        V( g_pEffect->SetValue( "g_nMaxSamples", &g_nMaxSamples, sizeof( int ) ) );
        V( g_pEffect->SetValue( "g_fShadowSoftening", &g_fShadowSoftening, sizeof( float ) ) );

        V( g_pEffect->SetBool( "g_bVisualizeLOD", ( BOOL )g_bVisualizeLOD ) );
        V( g_pEffect->SetBool( "g_bVisualizeMipLevel", ( BOOL )g_bVisualizeMipLevel ) );
        V( g_pEffect->SetBool( "g_bDisplayShadows", ( BOOL )g_bDisplayShadows ) );
        V( g_pEffect->SetBool( "g_bAddSpecular", ( BOOL )g_bAddSpecular ) );

        // Render the scene:
        switch( g_nCurrentTechniqueID )
        {
            case POM:
                V( g_pEffect->SetTechnique( "RenderSceneWithPOM" ) );
                break;

            case BUMPMAP:
                V( g_pEffect->SetTechnique( "RenderSceneWithBumpMap" ) );
                break;

            case PM:
                V( g_pEffect->SetTechnique( "RenderSceneWithPM" ) );
                break;
        }

        V( g_pEffect->Begin( &cPasses, 0 ) );
        for( iPass = 0; iPass < cPasses; iPass++ )
        {
            V( g_pEffect->BeginPass( iPass ) );
            V( g_pMesh->DrawSubset( 0 ) );
            V( g_pEffect->EndPass() );
        }
        V( g_pEffect->End() );
    }

    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );

    RenderText( fTime );

    V( pd3dDevice->EndScene() );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont 
// interface for efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText( double fTime )
{
    CDXUTTextHelper txtHelper( g_pFont, g_pSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 2, 0 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );

    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );

    // Draw help
    if( g_bShowHelp )
    {
        const D3DSURFACE_DESC* pd3dsdBackBuffer = DXUTGetD3D9BackBufferSurfaceDesc();
        txtHelper.SetInsertionPos( 2, pd3dsdBackBuffer->Height - 15 * 6 );
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Controls:" );

        txtHelper.SetInsertionPos( 20, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Rotate model: Left mouse button\n"
                                L"Rotate light: Right mouse button\n"
                                L"Rotate camera: Middle mouse button\n"
                                L"Zoom camera: Mouse wheel scroll\n" );

        txtHelper.SetInsertionPos( 250, pd3dsdBackBuffer->Height - 15 * 5 );
        txtHelper.DrawTextLine( L"Hide help: F1\n"
                                L"Quit: ESC\n" );
    }
    else
    {
        txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        txtHelper.DrawTextLine( L"Press F1 for help" );
    }
    txtHelper.End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd,
                          UINT uMsg,
                          WPARAM wParam,
                          LPARAM lParam,
                          bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
    {
        return 0;
    }

    // Pass the message to be handled to the light control:
    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
            {
                g_bShowHelp = !g_bShowHelp;
            }
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Switches to a specified pair of the POM texture pair from user input
//--------------------------------------------------------------------------------------
void SwitchPOMTextures( DXUTComboBoxItem* pSelectedItem )
{
    SetPOMTextures( ( int )( INT_PTR )pSelectedItem->pData );
    WCHAR strLabel[s_iMAX_STRING_SIZE];

    // Update the appropriate slider controls' bounds and values:
    CDXUTSlider* pHeightRangeSlider = g_SampleUI.GetSlider( IDC_HEIGHT_SCALE_SLIDER );
    if( pHeightRangeSlider )
    {
        pHeightRangeSlider->SetRange( g_iHeightScaleSliderMin, g_iHeightScaleSliderMax );
        pHeightRangeSlider->SetValue( ( int )( g_fHeightScale * g_fHeightScaleUIScale ) );

    }

    // Update the static parameter value:
    CDXUTStatic* pHeightRangeStatic = g_SampleUI.GetStatic( IDC_HEIGHT_SCALE_STATIC );
    if( pHeightRangeStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Height scale: %0.2f", g_fHeightScale );
        pHeightRangeStatic->SetText( strLabel );

    }

    // Update the appropriate slider controls' bounds and values:
    CDXUTSlider* pSpecularSlider = g_SampleUI.GetSlider( IDC_SPECULAR_EXPONENT_SLIDER );
    if( pSpecularSlider )
    {
        pSpecularSlider->SetValue( ( int )g_fSpecularExponent );

    }

    CDXUTStatic* pSpecularStatic = g_SampleUI.GetStatic( IDC_SPECULAR_EXPONENT_STATIC );
    if( pSpecularStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Specular exponent: %0.0f", g_fSpecularExponent );
        pSpecularStatic->SetText( strLabel );

    }

    // Mininum number of samples slider
    CDXUTSlider* pMinSlider = g_SampleUI.GetSlider( IDC_MIN_NUM_SAMPLES_SLIDER );
    if( pMinSlider )
    {
        pMinSlider->SetValue( g_nMinSamples );

    }

    CDXUTStatic* pMinStatic = g_SampleUI.GetStatic( IDC_MIN_NUM_SAMPLES_STATIC );
    if( pMinStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Minium number of samples: %d", g_nMinSamples );
        pMinStatic->SetText( strLabel );

    }

    // Maximum number of samples slider
    CDXUTSlider* pMaxSlider = g_SampleUI.GetSlider( IDC_MAX_NUM_SAMPLES_SLIDER );
    if( pMaxSlider )
    {
        pMaxSlider->SetValue( g_nMaxSamples );

    }

    CDXUTStatic* pMaxStatic = g_SampleUI.GetStatic( IDC_MAX_NUM_SAMPLES_STATIC );
    if( pMaxStatic )
    {
        swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Maximum number of samples: %d", g_nMaxSamples );
        pMaxStatic->SetText( strLabel );
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
        {
            DXUTToggleFullScreen();
            break;
        }

        case IDC_TOGGLEREF:
        {
            DXUTToggleREF();
            break;
        }

        case IDC_CHANGEDEVICE:
        {
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
        }

        case IDC_SPECULAR_EXPONENT_SLIDER:
        {
            g_fSpecularExponent = ( float )( g_SampleUI.GetSlider( IDC_SPECULAR_EXPONENT_SLIDER )->GetValue() );
            WCHAR sz[s_iMAX_STRING_SIZE];
            swprintf_s( sz, s_iMAX_STRING_SIZE, L"Specular exponent: %0.0f", g_fSpecularExponent );
            g_SampleUI.GetStatic( IDC_SPECULAR_EXPONENT_STATIC )->SetText( sz );
            break;
        }

        case IDC_HEIGHT_SCALE_SLIDER:
        {
            g_fHeightScale = ( float )( g_SampleUI.GetSlider( IDC_HEIGHT_SCALE_SLIDER )->GetValue() /
                                        g_fHeightScaleUIScale );
            WCHAR sz[s_iMAX_STRING_SIZE];
            swprintf_s( sz, s_iMAX_STRING_SIZE, L"Height scale: %0.2f", g_fHeightScale );
            g_SampleUI.GetStatic( IDC_HEIGHT_SCALE_STATIC )->SetText( sz );
            break;
        }

        case IDC_MIN_NUM_SAMPLES_SLIDER:
        {
            g_nMinSamples = g_SampleUI.GetSlider( IDC_MIN_NUM_SAMPLES_SLIDER )->GetValue();
            WCHAR strLabel[s_iMAX_STRING_SIZE];
            swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Minimum number of samples: %d", g_nMinSamples );
            g_SampleUI.GetStatic( IDC_MIN_NUM_SAMPLES_STATIC )->SetText( strLabel );
            break;
        }

        case IDC_MAX_NUM_SAMPLES_SLIDER:
        {
            g_nMaxSamples = g_SampleUI.GetSlider( IDC_MAX_NUM_SAMPLES_SLIDER )->GetValue();

            WCHAR strLabel[s_iMAX_STRING_SIZE];
            swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Maxinum number of samples: %d", g_nMaxSamples );
            g_SampleUI.GetStatic( IDC_MAX_NUM_SAMPLES_STATIC )->SetText( strLabel );
            break;
        }

        case IDC_TOGGLE_SHADOWS:
        {
            // Toggle shadows static button:
            g_bDisplayShadows = ( bool )( ( CDXUTCheckBox* )pControl )->GetChecked();

            WCHAR strLabel[s_iMAX_STRING_SIZE];
            if( g_bDisplayShadows )
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle self-occlusion shadows rendering: ON " );
            else
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle self-occlusion shadows rendering: OFF " );
            g_SampleUI.GetCheckBox( IDC_TOGGLE_SHADOWS )->SetText( strLabel );
            break;
        }

        case IDC_TECHNIQUE_COMBO_BOX:
        {
            DXUTComboBoxItem* pSelectedItem = ( ( CDXUTComboBox* )pControl )->GetSelectedItem();
            if( pSelectedItem )
                g_nCurrentTechniqueID = ( int )( INT_PTR )pSelectedItem->pData;

            break;
        }

        case IDC_TOGGLE_SPECULAR:
        {
            // Toggle specular static button:
            g_bAddSpecular = ( bool )( ( CDXUTCheckBox* )pControl )->GetChecked();

            WCHAR strLabel[s_iMAX_STRING_SIZE];
            if( g_bAddSpecular )
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle specular: ON" );
            else
                swprintf_s( strLabel, s_iMAX_STRING_SIZE, L"Toggle specular: OFF" );
            g_SampleUI.GetCheckBox( IDC_TOGGLE_SPECULAR )->SetText( strLabel );
            break;
        }

        case IDC_SELECT_TEXTURES_COMBOBOX:
        {
            DXUTComboBoxItem* pSelectedItem = ( ( CDXUTComboBox* )pControl )->GetSelectedItem();
            if( pSelectedItem )
                SwitchPOMTextures( pSelectedItem );
            break;
        }

        case IDC_RELOAD_BUTTON:
        {
            HRESULT hr;
            V( LoadEffectFile() );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9ResetDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9LostDevice();
    g_SettingsDlg.OnD3D9LostDevice();
    CDXUTDirectionWidget::StaticOnD3D9LostDevice();

    if( g_pFont ) g_pFont->OnLostDevice();
    if( g_pEffect ) g_pEffect->OnLostDevice();
    SAFE_RELEASE( g_pSprite );
}


//--------------------------------------------------------------------------------------
// Release D3D9 resources created in the OnD3D9CreateDevice callback 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D9DestroyDevice();
    g_SettingsDlg.OnD3D9DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D9DestroyDevice();

    // Release the memory used for meshes / fonts / effect / textures:
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pMesh );

    for( int iTextureIndex = POM_STONES; iTextureIndex < s_iNUM_TEXTURES; iTextureIndex++ )
    {
        SAFE_RELEASE( g_pBaseTextures[iTextureIndex] );
        SAFE_RELEASE( g_pNMHTextures[iTextureIndex] );
    }

    free( g_pBaseTextures );
    free( g_pNMHTextures );
}

#endif


