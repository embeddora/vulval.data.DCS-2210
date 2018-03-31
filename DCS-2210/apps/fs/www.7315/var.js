function GV(v,df)
{
  return (v.indexOf("<%") > -1 || v.indexOf("<@") > -1) ? df : v;
}
var g_isSupMpeg4=GV("<%supportmpeg4%>",1);
var g_videoFormat=GV("<%format%>",0);
var g_isPal=(GV("<%imagesource%>",1) == "1");
var g_deviceName=GV("<%devicename%>","VideoServer");
var g_defaultStorage=GV("<%defaultstorage%>",1); // 0: cf, 1:sd 255:no card
//var g_SDInsert=parseInt(GV("<%sdinsert%>",0));
var g_CFInsert=GV("<%cfinsert%>",0);
var g_cardGetLink=GV("<%defaultcardgethtm%>","sdget.htm");
var g_brandUrl=GV("<%brandurl%>",null);
var g_titleName=GV("<%title%>","IPCAM");
// 20090202 Netpool Add for D-Link Style
var g_softwareversion=GV("<%SoftwareVersion%>","1.00"); ;
var g_fullversion=GV("<%fullversion%>","1.00.00"); ;
//var g_brandName=GV("<%brandname%>".toLowerCase(),"nobrand");
var g_brandName="nobrand";
var g_supportTStamp=GV("<%supporttstamp%>",0);
var g_p1XSize=parseInt(GV("<%profile1xsize%>",320));
var g_p1YSize=parseInt(GV("<%profile1ysize%>",240));
var g_p2XSize=parseInt(GV("<%profile2xsize%>",320));
var g_p2YSize=parseInt(GV("<%profile2ysize%>",240));
var g_p3XSize=parseInt(GV("<%profile3xsize%>",320));
var g_p3YSize=parseInt(GV("<%profile3ysize%>",240));
var g_p4XSize=parseInt(GV("<%profile4xsize%>",320));
var g_p4YSize=parseInt(GV("<%profile4ysize%>",240));
var g_isSupP1=(parseInt(GV("<%supportprofile1%>",0)) >= 1);
var g_isSupP2=(parseInt(GV("<%supportprofile2%>",0)) >= 1);
var g_isSupP3=(parseInt(GV("<%supportprofile3%>",0)) >= 1);
var g_isSupP4=(parseInt(GV("<%supportprofile4%>",0)) >= 1);
var g_isSupAJAX1=(parseInt(GV("<%supportajax1%>",0)) >= 1);
var g_isSupAJAX2=(parseInt(GV("<%supportajax2%>",0)) >= 1);
var g_isSupAJAX3=(parseInt(GV("<%supportajax3%>",0)) >= 1);
var g_socketAuthority=parseInt(GV("<%socketauthority%>",3));  //0:admin,1:operator,2:viewer
var g_isAuthorityChange=(parseInt(GV("<%authoritychange%>",0)) == 1);
var g_isSupMotion=(parseInt(GV("<%supportmotion%>",0)) >= 1);
var g_isSupWireless=(parseInt(GV("<%supportwireless%>",0)) == 1);
var g_serviceFtpClient=parseInt(GV("<%serviceftpclient%>",0));
var g_serviceSmtpClient=parseInt(GV("<%servicesmtpclient%>",0));
var g_servicePPPoE=parseInt(GV("<%servicepppoe%>",0));
var g_serviceSNTPClient=parseInt(GV("<%servicesntpclient%>",0));
var g_serviceDDNSClient=parseInt(GV("<%serviceddnsclient%>",0));
//20081212 chirun add virtualserver
var g_serviceVirtualSClient=parseInt(GV("<%servicevirtualclient%>",0));

var g_s_maskarea=GV("<%supportmaskarea%>",0);
var g_machineCode="<%machinecode%>";
var g_maxCH=GV("<%maxchannel%>",1);
var g_isSupportRS485 = ("<%supportrs485%>"==1);
var g_isSupportRS232 = ("<%supportrs232%>"==1);
var g_useActiveX=GV("<%layoutnum.0%>",1);
var g_ptzID=GV("<%layoutnum.1%>",1);
var g_s_mui=GV("<%supportmui%>",1);
var g_mui=GV("<%mui%>",-1);
var g_isSupportSeq=("<%supportsequence%>"==1);
var g_isSupportMQ=(parseInt("<%quadmodeselect%>") >= 0);
var g_quadMode=GV("<%quadmodeselect%>",1); //default is 1:quad
//var g_isSupportSmtpAuth=(g_machineCode!="1290");
var g_isSupportSmtpAuth=true;
var g_isSupportIPFilter=("<%serviceipfilter%>"==1);
var g_oemFlag0=(parseInt(GV("<%oemflag0%>",0)));
var g_s_daynight=(parseInt(GV("<%supportdncontrol%>",0)) == 1);
if(g_s_daynight)
  var g_isIpcam = true;
else
  var g_isIpcam = false;  
var g_is264=(parseInt(GV("<%supportavc%>",0)) == 1);
var g_isSelMpeg4=(parseInt(GV("<%supportavc%>",0)) == 1);
var g_isSupportD2N=false;
var g_isSupportN2D=false;
var g_isSupportAudio=(parseInt(GV("<%supportaudio%>",0)) >= 1);
var g_isSupportptzpage=(parseInt(GV("<%supportptzpage%>",1)) == 1);
//var g_isShowPtzCtrl=((g_machineCode=="1670") && (g_socketAuthority < 2));
var g_isShowPtzCtrl=false;
//20090206 chirun add for 7313
var g_IMGStatus=1;
// 20090210 Netpool add to fix Img_htm
var g_stream1name = GV("<%stream1name%>","");
var g_stream2name = GV("<%stream2name%>","");
var g_stream3name = GV("<%stream3name%>","");
var g_stream4name = GV("<%stream4name%>","");

//Multi profile Case 1 --> for 7228,7227 Supprot H.264 and MJPEG
var g_isMP1 = (g_machineCode==1671 || g_machineCode==1771);
//Multi Profile for 73xx
var g_isMP73 = (g_machineCode==2001 || g_machineCode==2100 || g_machineCode==1679 || g_machineCode==1677 || g_machineCode==2000);
var g_enpantilt = (parseInt(GV("<%rs485enable%>",0)) == 1);

// 20090413 Netpool Modified for Multi Profile Setting
var g_numProfile = parseInt(GV("<%numprofile%>",3));
var mpMode = 1;
var g_motionBlock;
var g_audioType = GV("<%audiotype%>","G.726");
var g_Port = location.port;
var g_viewXSize;
var g_viewYSize;
var g_lockReconnenct = false;
var g_GetFPS = 15;
var g_GetProFileFPS = "getprofile1fps";
var g_isRunMotion = parseInt(GV("<%motionwizardconfig%>",1));
var g_modelname=GV("<%OEMModel%>","DCS-3710");
function loadJS(url)
{
  document.write('<sc'+'ript language="javascript" type="text/javascript" src="' + url + '"></script>');
}

