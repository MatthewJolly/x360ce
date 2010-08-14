/*  x360ce - XBOX360 Controler Emulator
*  Copyright (C) 2002-2010 ToCA Edit
*
*  x360ce is free software: you can redistribute it and/or modify it under the terms
*  of the GNU Lesser General Public License as published by the Free Software Found-
*  ation, either version 3 of the License, or (at your option) any later version.
*
*  x360ce is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
*  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
*  PURPOSE.  See the GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License along with x360ce.
*  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "globals.h"
#include "Utils.h"
#include "Config.h"
#include "DirectInput.h"
#include "FakeAPI.h"

//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------
DINPUT_DATA DDATA;
DINPUT_GAMEPAD Gamepad[4];	//but we need a 4 gamepads

INT init[4] = {NULL};

//-----------------------------------------------------------------------------

LPDIRECTINPUT8 GetDirectInput() {
	if (!DDATA.g_pDI) {
		if (FAILED(DirectInput8Create( hX360ceInstance, DIRECTINPUT_VERSION,IID_IDirectInput8, ( VOID** )&DDATA.g_pDI, NULL ) ) )
			return 0;
	}
	DDATA.refCount++;
	return DDATA.g_pDI;
}

void ReleaseDirectInput() 
{
	if (DDATA.refCount)  {
		DDATA.refCount--;
		if (!DDATA.refCount) {
			DDATA.g_pDI->Release();
			DDATA.g_pDI = 0;
		}
	}
}

void Deactivate(DWORD idx) 
{
	if (Gamepad[idx].ff.g_pEffect) {
		for (int i=0; i<2; i++) {
			if (Gamepad[idx].ff.g_pEffect[i]) {
				Gamepad[idx].ff.g_pEffect[i]->Stop();
				Gamepad[idx].ff.g_pEffect[i]->Release();
			}
			free(Gamepad[idx].ff.g_pEffect[i]);
			Gamepad[idx].ff.g_pEffect[i] = 0;
		}
	}
	if (Gamepad[idx].connected) {
		Gamepad[idx].g_pGamepad->Unacquire();
		Gamepad[idx].g_pGamepad->Release();
		Gamepad[idx].g_pGamepad = 0;
		Gamepad[idx].connected = 0;
	}
}

void Deactivate() 
{
	for (int i=0; i<4; i++) Deactivate(i);
}

BOOL CALLBACK EnumGamepadsCallback( const DIDEVICEINSTANCE* pInst,
	VOID* pContext )
{
	LPDIRECTINPUT8 lpDI8 = GetDirectInput();
	LPDIRECTINPUTDEVICE8 pDevice;
	DINPUT_GAMEPAD * gp = (DINPUT_GAMEPAD*) pContext;

	if(gp->product == pInst->guidProduct && gp->instance == pInst->guidInstance ) {
		lpDI8->CreateDevice( pInst->guidInstance, &pDevice, NULL );
		if(pDevice) {
			gp->g_pGamepad = pDevice;
			_tcscpy_s(gp->name,pInst->tszProductName);
			gp->connected = 1;
			WriteLog(_T("[DINPUT]  [PAD%d] Device \"%s\" initialized"),gp->dwPadIndex+1,gp->name);
		}
		return DIENUM_STOP;
	}
	else {
		return DIENUM_CONTINUE;
	}
}

BOOL CALLBACK EnumObjectsCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,VOID* pContext )
{

	DINPUT_GAMEPAD * gp = (DINPUT_GAMEPAD*) pContext;

	// For axes that are returned, set the DIPROP_RANGE property for the
	// enumerated axis in order to scale min/max values.
	if( pdidoi->dwType & DIDFT_AXIS ) {
		DIPROPRANGE diprg; 
		diprg.diph.dwSize       = sizeof(DIPROPRANGE); 
		diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
		diprg.diph.dwHow        = DIPH_BYID; 
		diprg.diph.dwObj        = pdidoi->dwType; // Specify the enumerated axis
		diprg.lMin              = -32767;
		diprg.lMax              = +32767; 

		// Set the range for the axis
		if( FAILED( gp->g_pGamepad->SetProperty( DIPROP_RANGE, &diprg.diph ) ) ) 
			return DIENUM_STOP;
	}
	gp->dwAxisCount++;
	return DIENUM_CONTINUE;
}

//-----------------------------------------------------------------------------
// Name: EnumFFAxesCallback()
// Desc: Callback function for enumerating the axes on a joystick and counting
//       each force feedback enabled axis
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumFFAxesCallback( const DIDEVICEOBJECTINSTANCE* pdidoi,VOID* pContext )
{
	DWORD* pdwNumForceFeedbackAxis = (DWORD*) pContext;

	if( ((pdidoi->dwFlags && DIDOI_FFACTUATOR) != 0) )
		(*pdwNumForceFeedbackAxis)++;

	return DIENUM_CONTINUE;
}

HRESULT UpdateState(DWORD idx )
{
	HRESULT hr=E_FAIL;

	if( (!Gamepad[idx].g_pGamepad))
		return E_FAIL;

	// Poll the device to read the current state
	// not all devices must be polled so checking result code is useless
	Gamepad[idx].g_pGamepad->Poll();

	//But GetDeviceState must be succesed
	hr = Gamepad[idx].g_pGamepad->GetDeviceState( sizeof( DIJOYSTATE2 ), &Gamepad[idx].state );
	if(FAILED(hr)) {
		if(bInitBeep) MessageBeep(MB_OK);
		WriteLog(_T("[DINPUT]  [PAD%d] Device Acquired"),idx+1);
		hr = Gamepad[idx].g_pGamepad->Acquire();
	}

	return hr;
}

HRESULT Enumerate(DWORD idx)
{
	HRESULT hr;

	Deactivate(idx);
	LPDIRECTINPUT8 lpDI8 = GetDirectInput();


	WORD wFakeModeOrig=wFakeMode;
	if (wFakeModeOrig) {
		wFakeMode=0; //Temporary disable FakeAPI
		WriteLog(_T("[DINPUT]  Temporary disable FakeAPI"));
	}
	WriteLog(_T("[DINPUT]  [PAD%d] Enumerating User ID %d"),idx+1,idx);
	hr = lpDI8->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumGamepadsCallback, &Gamepad[idx], DIEDFL_ATTACHEDONLY );
	if (wFakeModeOrig) {
		wFakeMode=wFakeModeOrig; // Restore FakeAPI state, if disable before
		WriteLog(_T("[DINPUT]  Restore FakeAPI state"));
	}
	if FAILED(hr) {
		WriteLog(_T("[DINPUT]  [PAD%d] Enumeration FAILED !!!"),idx+1);
		return hr;
	}
	if(!Gamepad[idx].g_pGamepad) WriteLog(_T("[PAD%d] Enumeration FAILED !!!"),idx+1);
	return ERROR_SUCCESS;
}

HRESULT InitDirectInput( HWND hDlg, INT idx )
{

	if(!Gamepad[idx].g_pGamepad) return ERROR_DEVICE_NOT_CONNECTED;

	DIPROPDWORD dipdw;
	HRESULT hr=S_OK;
	HRESULT coophr=S_OK;

	// Set the data format to "simple joystick" - a predefined data format. A
	// data format specifies which controls on a device we are interested in,
	// and how they should be reported.
	//
	// This tells DirectInput that we will be passing a DIJOYSTATE structure to
	// IDirectInputDevice8::GetDeviceState(). Even though we won't actually do
	// it in this sample. But setting the data format is important so that the
	// DIJOFS_* values work properly.
	if( FAILED( hr = Gamepad[idx].g_pGamepad->SetDataFormat( &c_dfDIJoystick2 ) ) ) {
		WriteLog(_T("[DINPUT]  [PAD%d] SetDataFormat failed with code HR = %s"), idx+1, DXErrStr(hr));
		return hr;
	}

	// Set the cooperative level to let DInput know how this device should
	// interact with the system and with other DInput applications.
	// Exclusive access is required in order to perform force feedback.
	if( FAILED( coophr = Gamepad[idx].g_pGamepad->SetCooperativeLevel( hDlg,
		DISCL_EXCLUSIVE |
		DISCL_BACKGROUND ) ) ) {
			WriteLog(_T("[DINPUT]  [PAD%d] SetCooperativeLevel (1) failed with code HR = %s"), idx+1, DXErrStr(coophr));
			//return coophr;
	}
	if(coophr!=S_OK) {
		WriteLog(_T("[DINPUT]  Device not exclusive acquired, disabling ForceFeedback"));
		Gamepad[idx].ff.useforce = 0;
		if( FAILED( coophr = Gamepad[idx].g_pGamepad->SetCooperativeLevel( hDlg,
			DISCL_NONEXCLUSIVE |
			DISCL_BACKGROUND ) ) ) {
				WriteLog(_T("[DINPUT]  [PAD%d] SetCooperativeLevel (2) failed with code HR = %s"), idx+1, DXErrStr(coophr));
				//return coophr;
		}
	}

	// Since we will be playing force feedback effects, we should disable the
	// auto-centering spring.
	dipdw.diph.dwSize = sizeof( DIPROPDWORD );
	dipdw.diph.dwHeaderSize = sizeof( DIPROPHEADER );
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = FALSE;
	// not all gamepad drivers need this (like PS3), so do not check result code
	Gamepad[idx].g_pGamepad->SetProperty( DIPROP_AUTOCENTER, &dipdw.diph );

	if( FAILED( hr = Gamepad[idx].g_pGamepad->EnumObjects( EnumObjectsCallback,
		( VOID* )&Gamepad[idx], DIDFT_AXIS ) ) ) {
			WriteLog(_T("[DINPUT]  [PAD%d] EnumObjects failed with code HR = %s"), idx+1, DXErrStr(hr));
			//return hr;
	}
	else {
		WriteLog(_T("[DINPUT]  [PAD%d] Detected axis count: %d"),idx+1,Gamepad[idx].dwAxisCount);
	}

	if( FAILED( hr = Gamepad[idx].g_pGamepad->EnumObjects( EnumFFAxesCallback,
		( VOID* )&Gamepad[idx].ff.g_dwNumForceFeedbackAxis, DIDFT_AXIS ) ) ) {
			WriteLog(_T("[DINPUT]  [PAD%d] EnumFFAxesCallback failed with code HR = %s"), idx+1, DXErrStr(hr));
			//return hr;
	}

	if( Gamepad[idx].ff.g_dwNumForceFeedbackAxis > 2 )
		Gamepad[idx].ff.g_dwNumForceFeedbackAxis = 2;

	if( Gamepad[idx].ff.g_dwNumForceFeedbackAxis <= 0 )
		Gamepad[idx].ff.useforce = 0;

	return S_OK;
}

HRESULT SetDeviceForces(DWORD idx, WORD force, WORD effidx)
{
	if(force) WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) %d"), idx+1,effidx, force);
	//[-10000:10000]
	//INT nForce = MulDiv(force, 2 * DI_FFNOMINALMAX, 65535) - DI_FFNOMINALMAX;
	//[0:10000]
	INT nForce = MulDiv(force, DI_FFNOMINALMAX, 65535);
	DWORD period;
	// Keep force within bounds
	nForce = clamp(nForce,-DI_FFNOMINALMAX,+DI_FFNOMINALMAX);

	if (effidx == 0) {
		Gamepad[idx].ff.xForce = nForce;
		period = 60000;
	}
	else {
		Gamepad[idx].ff.yForce = nForce;
		period = 120000;
	}
	DWORD magnitude = 0;
	// Constant:  Duration, Gain, TriggerButton, Axes, Direction, Envelope, TypeSpecificParams, StartDelay
	// Sine Wave: Duration, Gain, TriggerButton, Axes, Direction, Envelope, TypeSpecificParams, StartDelay, SamplePeriod
	HRESULT hr = S_OK;
	LONG rglDirection[2] = { 0, 0 };
	//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !1! HR = %s"), idx+1,effidx, DXErrStr(hr));
	DIEFFECT eff;
	if ( Gamepad[idx].ff.IsUpdateEffectCreated == false) {
		//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !1a! HR = %s"), idx+1,effidx, DXErrStr(hr));
		ZeroMemory( &eff, sizeof( eff ) );
		eff.dwSize = sizeof( DIEFFECT );
		eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
		eff.cAxes =  Gamepad[idx].ff.g_dwNumForceFeedbackAxis;
		eff.lpEnvelope = 0;
		eff.dwStartDelay = 0;
		//eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
		eff.cbTypeSpecificParams = sizeof( DIPERIODIC );
		Gamepad[idx].ff.eff = eff;
		Gamepad[idx].ff.IsUpdateEffectCreated = true;
	}
	eff =  Gamepad[idx].ff.eff;
	//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !2! HR = %s"), idx+1,effidx, DXErrStr(hr));
	// When modifying an effect you need only specify the parameters you are modifying
	if(  Gamepad[idx].ff.g_dwNumForceFeedbackAxis == 1 ) {
		//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !3a! HR = %s"), idx+1,effidx, DXErrStr(hr));
		// Apply only one direction and keep the direction at zero
		magnitude = ( DWORD )sqrt( ( double )Gamepad[idx].ff.xForce * ( double )Gamepad[idx].ff.xForce + ( double )Gamepad[idx].ff.yForce * ( double )Gamepad[idx].ff.yForce );
		rglDirection[0] = 0;
		Gamepad[idx].ff.pf.dwMagnitude = Gamepad[idx].ff.xForce;
		Gamepad[idx].ff.pf.dwPeriod = period;
	}
	else {
		//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !3b! HR = %s"), idx+1,effidx, DXErrStr(hr));
		magnitude = MulDiv(force, DI_FFNOMINALMAX, 65535);
		// Apply magnitude from both directions 
		rglDirection[0] = Gamepad[idx].ff.xForce;
		rglDirection[1] = Gamepad[idx].ff.yForce;
		Gamepad[idx].ff.pf.dwMagnitude = magnitude;
		Gamepad[idx].ff.pf.dwPeriod = period;
		//LeftForceMagnitude
		//LeftForcePeriod
		// dwMagnitude - Magnitude of the effect, in the range from 0 through 10,000. If an envelope is applied to this effect, the value represents the magnitude of the sustain. If no envelope is applied, the value represents the amplitude of the entire effect. 
		// lOffset - Offset of the effect. The range of forces generated by the effect is lOffset minus dwMagnitude to lOffset plus dwMagnitude. The value of the lOffset member is also the baseline for any envelope that is applied to the effect. 
		// dwPhase - Position in the cycle of the periodic effect at which playback begins, in the range from 0 through 35,999. See Remarks. 
		// dwPeriod - Period of the effect, in microseconds. 
	}
	//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !3b! axis = %d, x = %d, y = %d, m = %d"), idx+1,effidx, Gamepad[idx].g_dwNumForceFeedbackAxis, Gamepad[idx].xForce, Gamepad[idx].yForce, magnitude);
	//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !6! HR = %s"), idx+1,effidx, DXErrStr(hr));
	Gamepad[idx].ff.eff.rglDirection = rglDirection;
	Gamepad[idx].ff.eff.lpvTypeSpecificParams = &Gamepad[idx].ff.pf;
	if ( Gamepad[idx].ff.oldMagnitude != Gamepad[idx].ff.pf.dwMagnitude ||
		Gamepad[idx].ff.oldPeriod !=  Gamepad[idx].ff.pf.dwPeriod ||
		Gamepad[idx].ff.oldXForce != Gamepad[idx].ff.xForce ||
		Gamepad[idx].ff.oldYForce !=  Gamepad[idx].ff.yForce) {
			Gamepad[idx].ff.oldMagnitude =  Gamepad[idx].ff.pf.dwMagnitude;
			Gamepad[idx].ff.oldPeriod =  Gamepad[idx].ff.pf.dwPeriod;
			Gamepad[idx].ff.oldXForce = Gamepad[idx].ff.xForce;
			Gamepad[idx].ff.oldYForce = Gamepad[idx].ff.yForce;
			//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) !7! HR = %s"), idx+1,effidx, DXErrStr(hr));
			// Set the new parameters and start the effect immediately.
			if( FAILED( hr = Gamepad[idx].ff.g_pEffect[effidx]->SetParameters( &Gamepad[idx].ff.eff, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START ))){
				WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) failed with code HR = %s"), idx+1,effidx, DXErrStr(hr));
				return hr;
			};
	}
	//WriteLog(_T("[DINPUT]  [PAD%d] SetDeviceForces (%d) return HR = %s"), idx+1,effidx, DXErrStr(hr));
	return hr;
}

//-----------------------------------------------------------------------------
// Name: PrepareDeviceForces()
// Desc: Prepare force feedback effect.
//-----------------------------------------------------------------------------
HRESULT PrepareForce(DWORD idx, WORD effidx)
{
	DIEFFECT eff;
	DIPERIODIC pf = { 0 };
	Gamepad[idx].ff.pf = pf;
	Gamepad[idx].ff.xForce = 0;
	Gamepad[idx].ff.yForce = 0;
	Gamepad[idx].ff.oldXForce = 0;
	Gamepad[idx].ff.oldYForce = 0;
	Gamepad[idx].ff.oldMagnitude = 0;
	Gamepad[idx].ff.oldPeriod = 0;
	Gamepad[idx].ff.IsUpdateEffectCreated = false;
	// Constant:  Duration, Gain, TriggerButton, Axes, Direction, Envelope, TypeSpecificParams, StartDelay
	// Sine Wave: Duration, Gain, TriggerButton, Axes, Direction, Envelope, TypeSpecificParams, StartDelay, SamplePeriod
	HRESULT hr = E_FAIL;
	LONG rglDirection[2] = { 0, 0 };
	// Create effect
	ZeroMemory( &eff, sizeof( eff ) );
	eff.dwSize = sizeof( DIEFFECT );
	eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
	eff.cAxes = Gamepad[idx].ff.g_dwNumForceFeedbackAxis;
	eff.lpEnvelope = 0;
	eff.dwStartDelay = 0;
	eff.cbTypeSpecificParams = sizeof( DIPERIODIC );
	GUID effGuid = GUID_Sine;
	// Force feedback
	DIDEVCAPS didcaps;
	didcaps.dwSize = sizeof didcaps;
	if (SUCCEEDED(Gamepad[idx].g_pGamepad->GetCapabilities(&didcaps)) && (didcaps.dwFlags & DIDC_FORCEFEEDBACK)) {
		WriteLog(_T("[DINPUT]  [PAD%d] PrepareForce (%d) Force Feedback is available"), idx+1,effidx);
	} 
	else {
		WriteLog(_T("[DINPUT]  [PAD%d] PrepareForce (%d) Force Feedback is NOT available"), idx+1,effidx);
	}
	// Enumerate effects
	if (SUCCEEDED(hr = Gamepad[idx].g_pGamepad->EnumEffects(&EnumEffectsCallback, Gamepad[idx].g_pGamepad, DIEFT_ALL))) {
	} 
	else {
	}
	// This application needs only one effect: Applying raw forces.
	DWORD rgdwAxes[2] = { DIJOFS_X, DIJOFS_Y };
	eff.dwDuration = INFINITE;
	eff.dwSamplePeriod = 0;
	eff.dwGain = DI_FFNOMINALMAX; // no scaling
	eff.dwTriggerButton = DIEB_NOTRIGGER;
	eff.dwTriggerRepeatInterval = 0;
	eff.rgdwAxes = rgdwAxes;
	eff.rglDirection = rglDirection;
	//eff.lpvTypeSpecificParams = &cf;
	eff.lpvTypeSpecificParams = &Gamepad[idx].ff.pf;
	// Create the prepared effect
	if( FAILED( hr = Gamepad[idx].g_pGamepad->CreateEffect(
		effGuid,  // GUID from enumeration
		&eff, // where the data is
		&Gamepad[idx].ff.g_pEffect[effidx],  // where to put interface pointer
		NULL)))
	{
		WriteLog(_T("[DINPUT]  [PAD%d] PrepareForce (%d) failed with code HR = %s"), idx+1,effidx, DXErrStr(hr));
		return hr;
	}
	if(Gamepad[idx].ff.g_pEffect[effidx] == NULL) return E_FAIL;
	//WriteLog(_T("[DINPUT]  [PAD%d] PrepareForce (%d) HR = %s"), idx+1,effidx, DXErrStr(hr));
	return S_OK;
}

//-----------------------------------------------------------------------------
// enumEffects is called by the operating system for each force
// feedback effect available on the game controller device.  The
// first parameter points to information about the effect, including
// its GUID, the second parameter points to user supplied data, which
// is the combo box.  This function adds the name of the effect as
// a line item in the combo box along with the address of the memory
// allocated for storing the GUID.
//-----------------------------------------------------------------------------
BOOL CALLBACK EnumEffectsCallback(LPCDIEFFECTINFO di, LPVOID pvRef) {
	//    LPDIRECTINPUTDEVICE8 g_pDevice = (LPDIRECTINPUTDEVICE8)pvRef; 
	UNREFERENCED_PARAMETER(pvRef);
	// Pointer to calling device
	BOOL isConstant = DIEFT_GETTYPE(di->dwEffType) == DIEFT_CONSTANTFORCE;
	BOOL isPeriodic = DIEFT_GETTYPE(di->dwEffType) == DIEFT_PERIODIC;
	WriteLog(_T("   Effect '%s'. IsConstant = %d, IsPeriodic = %d"), di->tszName, isConstant, isPeriodic);
	return DIENUM_CONTINUE;
}

// return buttons state (1 pressed, 0 not pressed)
BOOL ButtonPressed(DWORD buttonidx, INT idx) 
{
	return (Gamepad[idx].state.rgbButtons[buttonidx] & 0x80) != 0;
}
