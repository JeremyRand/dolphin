// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <list>
#include <d3dx9.h>

#include "Common.h"
#include "Statistics.h"

#include "VideoConfig.h"
#include "main.h"
#include "VertexManager.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "XFStructs.h"
#include "D3DUtil.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderCache.h"
#include "VertexLoaderManager.h"
#include "TextureCache.h"
#include "Utils.h"
#include "EmuWindow.h"
#include "AVIDump.h"
#include "OnScreenDisplay.h"
#include "FramebufferManager.h"
#include "Fifo.h"

#include "debugger/debugger.h"

static int s_target_width;
static int s_target_height;

static int s_backbuffer_width;
static int s_backbuffer_height;

static float xScale;
static float yScale;

static int s_recordWidth;
static int s_recordHeight;

static bool s_LastFrameDumped;
static bool s_AVIDumping;

#define NUMWNDRES 6
extern int g_Res[NUMWNDRES][2];
char st[32768];

void SetupDeviceObjects()
{
	D3D::font.Init();
	VertexLoaderManager::Init();
	FBManager::Create();

	VertexShaderManager::Dirty();
	PixelShaderManager::Dirty();

	// Tex and shader caches will recreate themselves over time.
}

// Kill off all POOL_DEFAULT device objects.
void TeardownDeviceObjects()
{
	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(D3D::GetBackBufferDepthSurface());
	FBManager::Destroy();
	D3D::font.Shutdown();
	TextureCache::Invalidate(false);
	VertexManager::DestroyDeviceObjects();
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Clear();
	PixelShaderCache::Clear();
}

bool Renderer::Init() 
{
	UpdateActiveConfig();
	int fullScreenRes, w_temp, h_temp;
	sscanf(g_Config.cInternalRes, "%dx%d", &w_temp, &h_temp);
	if (w_temp <= 0 || h_temp <= 0)
	{
		w_temp = 640;
		h_temp = 480;
	}
	EmuWindow::SetSize(w_temp, h_temp);

	int backbuffer_ms_mode = 0;  // g_ActiveConfig.iMultisampleMode;

	sscanf(g_Config.cFSResolution, "%dx%d", &w_temp, &h_temp);

	for (fullScreenRes = 0; fullScreenRes < D3D::GetNumAdapters(); fullScreenRes++)
	{
		if ((D3D::GetAdapter(fullScreenRes).resolutions[fullScreenRes].xres == w_temp) && 
			(D3D::GetAdapter(fullScreenRes).resolutions[fullScreenRes].yres == h_temp))
			break;
	}
	D3D::Create(g_ActiveConfig.iAdapter, EmuWindow::GetWnd(), g_ActiveConfig.bFullscreen,
				fullScreenRes, backbuffer_ms_mode, false);

	s_backbuffer_width = D3D::GetBackBufferWidth();
	s_backbuffer_height = D3D::GetBackBufferHeight();

	// TODO: Grab target width from configured resolution?
	s_target_width  = s_backbuffer_width * EFB_WIDTH / 640;
	s_target_height = s_backbuffer_height * EFB_HEIGHT / 480;

	xScale = (float)s_target_width / (float)EFB_WIDTH;
	yScale = (float)s_target_height / (float)EFB_HEIGHT;

	s_LastFrameDumped = false;
	s_AVIDumping = false;

	// We're not using fixed function, except for some 2D.
	// Let's just set the matrices to identity to be sure.
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW, &mtx);
	D3D::dev->SetTransform(D3DTS_WORLD, &mtx);

	SetupDeviceObjects();

	for (int stage = 0; stage < 8; stage++)
		D3D::SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, g_ActiveConfig.iMaxAnisotropy);

	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x0, 0, 0);

	D3D::dev->SetRenderTarget(0, FBManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FBManager::GetEFBDepthRTSurface());

	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x0, 1.0f, 0);

	D3D::BeginFrame();
	return true;
}

void Renderer::Shutdown()
{
	TeardownDeviceObjects();
	D3D::EndFrame();
	D3D::Close();

	if (s_AVIDumping)
	{
		AVIDump::Stop();
	}
}

int Renderer::GetTargetWidth() { return s_target_width; }
int Renderer::GetTargetHeight() { return s_target_height; }
float Renderer::GetTargetScaleX() { return xScale; }
float Renderer::GetTargetScaleY() { return yScale; }

void Renderer::RenderText(const char *text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20, 20, 0.0f, color, text, false);
}

void dumpMatrix(D3DXMATRIX &mtx)
{
	for (int y = 0; y < 4; y++)
	{
		char temp[256];
		sprintf(temp,"%4.4f %4.4f %4.4f %4.4f",mtx.m[y][0],mtx.m[y][1],mtx.m[y][2],mtx.m[y][3]);
		g_VideoInitialize.pLog(temp, FALSE);
	}
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = (rc.left   * s_target_width)  / EFB_WIDTH;
	result.top    = (rc.top    * s_target_height) / EFB_HEIGHT;
	result.right  = (rc.right  * s_target_width)  / EFB_WIDTH;
	result.bottom = (rc.bottom * s_target_height) / EFB_HEIGHT;
	return result;
}

void formatBufferDump(const char *in, char *out, int w, int h, int p)
{
	for (int y = 0; y < h; y++)
	{
		const char *line = in + (h - y - 1) * p;
		for (int x = 0; x < w; x++)
		{
			memcpy(out, line, 3);
			out += 3;
			line += 4;
		}
	}
}

// With D3D, we have to resize the backbuffer if the window changed
// size.
void CheckForResize()
{
	while (EmuWindow::IsSizing())
	{
		Sleep(10);
	}

	RECT rcWindow;
	GetClientRect(EmuWindow::GetWnd(), &rcWindow);
	int client_width = rcWindow.right - rcWindow.left;
	int client_height = rcWindow.bottom - rcWindow.top;
	// Sanity check.
	if ((client_width != s_backbuffer_width ||
		client_height != s_backbuffer_height) && 
		client_width >= 4 && client_height >= 4)
	{
		TeardownDeviceObjects();

		D3D::Reset();

		SetupDeviceObjects();
		s_backbuffer_width = D3D::GetBackBufferWidth();
		s_backbuffer_height = D3D::GetBackBufferHeight();
	}
}


void Renderer::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	if (g_bSkipCurrentFrame)
	{
		g_VideoInitialize.pCopiedToXFB(false);
		return;
	}

	if (EmuWindow::GetParentWnd())
	{
		// Re-stretch window to parent window size again, if it has a parent window.
		RECT rcWindow;
		GetWindowRect(EmuWindow::GetParentWnd(), &rcWindow);

		int width = rcWindow.right - rcWindow.left;
		int height = rcWindow.bottom - rcWindow.top;

		::MoveWindow(EmuWindow::GetWnd(), 0, 0, width, height, FALSE);
	}

	// Frame dumping routine - seems buggy and wrong, esp. regarding buffer sizes
	if (g_ActiveConfig.bDumpFrames) {
		D3DDISPLAYMODE DisplayMode;
		if (SUCCEEDED(D3D::dev->GetDisplayMode(0, &DisplayMode))) {
			LPDIRECT3DSURFACE9 surf;
			if (SUCCEEDED(D3D::dev->CreateOffscreenPlainSurface(DisplayMode.Width, DisplayMode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &surf, NULL))) {
				if (!s_LastFrameDumped) {
					RECT windowRect;
					GetClientRect(EmuWindow::GetWnd(), &windowRect);
					s_recordWidth = windowRect.right - windowRect.left;
					s_recordHeight = windowRect.bottom - windowRect.top;
					s_AVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), s_recordWidth, s_recordHeight);
					if (!s_AVIDumping) {
						PanicAlert("Error dumping frames to AVI.");
					} else {
						char msg [255];
						sprintf(msg, "Dumping Frames to \"%s/framedump0.avi\" (%dx%d RGB24)", FULL_FRAMES_DIR, s_recordWidth, s_recordHeight);
						OSD::AddMessage(msg, 2000);
					}
				}
				if (s_AVIDumping) {
					if (SUCCEEDED(D3D::dev->GetFrontBufferData(0, surf))) {
						RECT windowRect;
						GetWindowRect(EmuWindow::GetWnd(), &windowRect);
						D3DLOCKED_RECT rect;
						if (SUCCEEDED(surf->LockRect(&rect, &windowRect, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY))) {
							char *data = (char *)malloc(3 * s_recordWidth * s_recordHeight);
							formatBufferDump((const char *)rect.pBits, data, s_recordWidth, s_recordHeight, rect.Pitch);
							AVIDump::AddFrame(data);
							free(data);
							surf->UnlockRect();
						}
					}
				}
				s_LastFrameDumped = true;
				surf->Release();
			}
		}
	} 
	else 
	{
		if (s_LastFrameDumped && s_AVIDumping) {
			AVIDump::Stop();
			s_AVIDumping = false;
		}

		s_LastFrameDumped = false;
	}

	D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
	D3D::dev->SetDepthStencilSurface(NULL);

	// Blit our render target onto the backbuffer.
	// TODO: Change to a quad so we can do post processing.
	TargetRectangle src_rect, dst_rect;
	src_rect = ConvertEFBRectangle(sourceRc);
	ComputeDrawRectangle(s_backbuffer_width, s_backbuffer_height, false, &dst_rect);
	D3D::dev->StretchRect(FBManager::GetEFBColorRTSurface(), src_rect.AsRECT(),
		                  D3D::GetBackBufferSurface(), dst_rect.AsRECT(),
						  D3DTEXF_LINEAR);

	// Finish up the current frame, print some stats
	if (g_ActiveConfig.bOverlayStats)
	{
		Statistics::ToString(st);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);
	}
	else if (g_ActiveConfig.bOverlayProjStats)
	{
		Statistics::ToStringProj(st);
		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);
	}

	OSD::DrawMessages();
	D3D::EndFrame();

	DEBUGGER_PAUSE_COUNT_N_WITHOUT_UPDATE(NEXT_FRAME);

	// D3D frame is now over
	// Clean out old stuff from caches.
	frameCount++;
	PixelShaderCache::Cleanup();
	VertexShaderCache::Cleanup();
	TextureCache::Cleanup();

	// Make any new configuration settings active.
	UpdateActiveConfig();

	// TODO: Resize backbuffer if window size has changed. This code crashes, debug it.
	g_VideoInitialize.pCopiedToXFB(false);

	CheckForResize();


	// Begin new frame
	// Set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();
	// u32 clearColor = (bpmem.clearcolorAR << 16) | bpmem.clearcolorGB;
	D3D::BeginFrame();

	// Clear backbuffer. We probably don't need to do this every frame.
	D3D::dev->Clear(0, NULL, D3DCLEAR_TARGET, 0x0, 1.0f, 0);

	D3D::dev->SetRenderTarget(0, FBManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FBManager::GetEFBDepthRTSurface());

	RECT rc;
	rc.left   = 0; 
	rc.top    = 0;
	rc.right  = (LONG)s_target_width;
	rc.bottom = (LONG)s_target_height;
	D3D::dev->SetScissorRect(&rc);
	D3D::SetRenderState(D3DRS_SCISSORTESTENABLE, false);

	UpdateViewport();
}

bool Renderer::SetScissorRect()
{
	int xoff = bpmem.scissorOffset.x * 2 - 342;
	int yoff = bpmem.scissorOffset.y * 2 - 342;
	RECT rc;
	rc.left   = (int)((float)bpmem.scissorTL.x - xoff - 342);
	rc.top    = (int)((float)bpmem.scissorTL.y - yoff - 342);
	rc.right  = (int)((float)bpmem.scissorBR.x - xoff - 341);
	rc.bottom = (int)((float)bpmem.scissorBR.y - yoff - 341);

	rc.left   = (int)(rc.left   * xScale);
	rc.top    = (int)(rc.top    * yScale);
	rc.right  = (int)(rc.right  * xScale);
	rc.bottom = (int)(rc.bottom * yScale);

	if (rc.left < 0) rc.left = 0;
	if (rc.right > s_target_width) rc.right = s_target_width;
	if (rc.top < 0) rc.top = 0;
	if (rc.bottom > s_target_height) rc.bottom = s_target_height;

	if (rc.right >= rc.left && rc.bottom >= rc.top)
	{
		D3D::dev->SetScissorRect(&rc);
		return true;
	}
	else
	{
		WARN_LOG(VIDEO, "Bad scissor rectangle: %i %i %i %i", rc.left, rc.top, rc.right, rc.bottom);
		return false;
	}
}

void Renderer::SetColorMask() 
{
	DWORD color_mask = 0;
	if (bpmem.blendmode.alphaupdate) 
		color_mask = D3DCOLORWRITEENABLE_ALPHA;
	if (bpmem.blendmode.colorupdate) 
		color_mask |= D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE;
	D3D::SetRenderState(D3DRS_COLORWRITEENABLE, color_mask);
}

u32 Renderer::AccessEFB(EFBAccessType type, int x, int y)
{
	// Get the rectangular target region covered by the EFB pixel.
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;

	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	// TODO (FIX) : currently, AA path is broken/offset and doesn't return the correct pixel
	switch (type)
	{
	case PEEK_Z:
	{
		// if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			// g_framebufferManager.GetEFBDepthTexture(efbPixelRc);
			// glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		u32 z = 0;
		// glReadPixels(srcX, srcY, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &z);

		// Scale the 32-bit value returned by glReadPixels to a 24-bit
		// value (GC uses a 24-bit Z-buffer).
		// TODO: in RE0 this value is often off by one, which causes lighting to disappear
		return z >> 8;
	}

	case POKE_Z:
		// TODO: Implement
		break;

	case PEEK_COLOR: // GXPeekARGB
	{
		// Although it may sound strange, this really is A8R8G8B8 and not RGBA or 24-bit...

		// Tested in Killer 7, the first 8bits represent the alpha value which is used to
		// determine if we're aiming at an enemy (0x80 / 0x88) or not (0x70) 
		// Wind Waker is also using it for the pictograph to determine the color of each pixel

		// if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			// g_framebufferManager.GetEFBColorTexture(efbPixelRc);
			// glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		// Read back pixel in BGRA format, then byteswap to get GameCube's ARGB Format.
		u32 color = 0;
		// glReadPixels(srcX, srcY, 1, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &color);

		return color;
	}

	case POKE_COLOR:
		// TODO: Implement. One way is to draw a tiny pixel-sized rectangle at
		// the exact location. Note: EFB pokes are susceptible to Z-buffering
		// and perhaps blending.
		// WARN_LOG(VIDEOINTERFACE, "This is probably some kind of software rendering");
		break;
	}

	return 0;
}

//		mtx.m[0][3] = pMatrix[1]; // -0.5f/s_target_width;  <-- fix d3d pixel center?
//		mtx.m[1][3] = pMatrix[3]; // +0.5f/s_target_height; <-- fix d3d pixel center?

// Called from VertexShaderManager
void UpdateViewport()
{
	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();

	D3DVIEWPORT9 vp;

	// Stretch picture with increased internal resolution
	vp.X = (int)(ceil(xfregs.rawViewport[3] - xfregs.rawViewport[0] - (scissorXOff)) * MValueX);
	vp.Y = (int)(ceil(xfregs.rawViewport[4] + xfregs.rawViewport[1] - (scissorYOff)) * MValueY);
	
	vp.Width  = (int)ceil(abs((int)(2 * xfregs.rawViewport[0])) * MValueX);
	vp.Height = (int)ceil(abs((int)(2 * xfregs.rawViewport[1])) * MValueY);
	
	vp.MinZ = (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777215.0f;
	vp.MaxZ = xfregs.rawViewport[5] / 16777215.0f;
	
	// This seems to happen a lot - the above calc is probably wrong.
	if (vp.MinZ < 0.0f) vp.MinZ = 0.0f;
	if (vp.MaxZ > 1.0f) vp.MaxZ = 1.0f;
	
	D3D::dev->SetViewport(&vp);
}
