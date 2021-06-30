const VER = "ver100";

let cameraUrl: string | undefined = undefined;

function apiCall<T>(
  endpoint: string,
  body: Object = {},
  method: "get" | "post" | "put" = "get"
): Promise<T> {
  const ccapiurl = endpoint
    ? `${cameraUrl}/ccapi/${VER}${endpoint}`
    : `${cameraUrl}/ccapi`;
  const url = `/proxy/?url=${ccapiurl}`;
  // console.log(body, method)
  return (
    fetch(url, {
      method,
      headers: {
        'Content-Type': 'application/json'
      },
      // redirect: 'follow',
      // cache: 'no-cache',
      // mode: 'no-cors',
      body: method != 'get' ? JSON.stringify(body) : undefined,
    })
      // .then( res => {
      //   console.log(res)
      //   return res
      // })
      .then((res) => res.json())
      .catch((err) => {
        console.error(err);
        throw err;
      })
  );
}

export function connectCamera(_cameraUrl: string) {
  cameraUrl = _cameraUrl;
  return apiCall("").then((res) => {
    // console.log(res);
    return true;
  });
}

export interface DeviceInformationResponse {
  manufacturer: string;
  productname: string;
  guid: string;
  serialnumber: string;
  macaddress: string;
  firmwareversion: string;
}

export function getDeviceInformation() {
  return apiCall<DeviceInformationResponse>("/deviceinformation");
}

export interface DeviceStatusStorageResponse {
  name: string;
  url: string;
  accesscapability: "readwrite" | "readonly";
  maxsize: number;
  spacesize: number;
  contentsnumber: number;
}
export function getDeviceStatusStorage() {
  return apiCall<DeviceStatusStorageResponse>("/devicestatus/storage");
}

export function getStorageContent(storage) {
  return apiCall<string[]>(`/contents/${storage}`);
}

export function getDirectoryContent(storage, directory) {
  return apiCall<string[]>(`/contents/${storage}/${directory}`);
}

export interface DeviceStatusCurrentDirectoryResponse {
  name: string;
  path: string;
}
export function getDeviceStatusCurrentDirectory() {
  return apiCall<DeviceStatusCurrentDirectoryResponse>(
    "/devicestatus/currentdirectory"
  );
}

export interface DeviceStatusBatteryResponse {
  name: string;
  kind: string;
  level:
  | "low"
  | "half"
  | "high"
  | "full"
  | "unknown"
  | "charge"
  | "chargestop"
  | "chargecomp";
  quality: "bad" | "normal" | "good" | "unknown";
}
export function getDeviceStatusBattery() {
  return apiCall<DeviceStatusBatteryResponse>("/devicestatus/battery");
}

export interface DeviceStatusLensResponse {
  name: string;
  mount: string;
}
export function getDeviceStatusLens() {
  return apiCall<DeviceStatusLensResponse>("/devicestatus/lens");
}

export function shootingControlShutterButton(af: boolean) {
  return apiCall<{ message?: string }>("/shooting/control/shutterbutton", { af }, 'post');
}

export function shootingControlShutterButtonManual(action: 'release' | 'half_press' | 'full_press', af: boolean) {
  return apiCall<{ message?: string }>("/shooting/control/shutterbutton/manual", { af, action }, 'post');
}


export interface ShootingSettingsResponse {
  shootingmodedial: { value: string },
  av: { value: string }
  tv: { value: string }
  wb: { value: string }
  iso: { value: string }
  exposure: { value: string }
}

export function getShootingSettings() {
  return apiCall<ShootingSettingsResponse>("/shooting/settings/");
}


export function shootingLiveView(
  liveviewsize: "off" | "small" | "medium",
  cameradisplay: "on" | "off" | "keep" = "keep"
) {
  return apiCall<[]>(
    "/shooting/liveview",
    { liveviewsize, cameradisplay },
    "post"
  );
}

export function parseTime(time: string): number {
  const match1 = time.match(/(\d+)"(\d+)?/im)
  const match2 = time.match(/(\d+)\/(\d+)/im)
  let ret = 0;
  if (match1) {
    ret = parseInt(match1[1]) * 1000;
    if (match1[2])
      ret += parseInt(match1[2]) * 100
  } else if (match2) {
    ret = 1000 * parseInt(match2[1]) / parseInt(match2[2])
  }

  return ret
}

export function shootingSettingsDrive(value: "single" | "cont_super_hi" | "highspeed" | "lowspeed" | "self_10sec" | "self_2sec" | "self_continuous") {
  return apiCall<[]>(
    "/shooting/settings/drive",
    { value: value },
    "put"
  );
}

export function shootingSettingsStillImageQuality(jpeg: "none" | "large_fine" | "large_normal" | "medium_fine" | "medium_normal" | "small1_fine" | "small1_normal" | "small2", raw: 'none' | 'raw' | 'craw' = 'none') {
  return apiCall<[]>(
    "/shooting/settings/stillimagequality",
    {
      value: {
        raw,
        jpeg
      }
    },
    "put"
  );
}

export interface PollingResponse {
  addedcontents?: string[],
  shuttermode?: Object,
  battery?: DeviceStatusBatteryResponse,
}
export function polling(wait) {
  return apiCall<PollingResponse>(`/event/polling?continue=${wait ? 'on' : 'off'}`)
}