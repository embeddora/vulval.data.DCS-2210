
var AxVer="1,0,0,16";  //this is for Java Web Page Maker to replace.
var webPageVersion="D-Link1.0";  //this is for Java Web Page Maker to replace.
var webPageSubVersion="72";  //this is for Java Web Page Maker to replace.
var DigSignName="D-Link"; //the name of the digital signature
var g_viewXSize=0;
var g_viewYSize=0;
var browser_Netscape=false;
var browser_IE=false;
var browser_FireFox=false;
var keyword_Show='';
var keyword_Hide='';
var AxID="VideoCtrl";
var AxRoomID="VideoAxRoom";
var g_MaxSubmitLen=190; // it will be over, increase 30
var AxUuid="79F8B9ED-56FA-4F6D-95FE-5FDEB26EA94F";  //this is for Java Web Page Maker to replace.
var LAry = {};
var LangKeyPrefix = "lang_";
var g_CHID=0;
var CONTENT_PAGE='';
var CONTENT_PAGE_LAST=''; //for link error backup.
var CTRLARY = {};
var c_iniUrl = "/vb.htm?language=ie";
var g_httpOK = true;
var g_SubmitHttp = null;
var g_lockLink = false;  // lock the link , can not access.
var g_CHPageList = "c_sccd.htm;sdigital.htm;aenable.htm;motion.htm;img.htm;imgtune.htm;main.htm;";
var g_badLinkList="^bwcntl\\.htm;^image\\.htm;^update\\.htm;^nftphost\\.htm;^help\\.htm;^ndhcpsvr\\.htm;^nupnp\\.htm;^k_\\w*\\.htm;^p_\\w*\\.htm;^ptz_\\w*\\.htm;^faq\\.htm;^version\\.htm;^index\\.htm;^sccd\\.htm;^armio\\.htm;^svideo\\.htm;^tailpage\\.htm;^mcenter\\.htm;^lang\\.htm".split(";");
var g_AdvMode = 0; //display advance menu. 1:on , 0:off
var g_lastPolicy = 0;
var WCH=null; //WebContentHttp
//set language name , must has order.
var g_langFullNameList=new Array("en_us","zh_tw","zh_cn","cs_cz","nl_nl","fi_fi","fr_fr","de_de","it_it","pl_pl","pt_pt","es_es","sv_se","hu_hu","ro_ro");
var g_langNameList;
var g_langName;
var g_sh_net=true;
var g_sh_pppoe=true;
var g_backList=new Array();
var g_fwdList=new Array();
var NO_STORAGE = 255; //255
var ISNOSTORE=(g_defaultStorage == NO_STORAGE);
//add vt specify , video server not limited.
//it is a stupid way, to check this.
//var g_X=(g_brandName=="vt")&&(!IsVS());
//g_X=false;
var g_isShowUpdate  =( (g_oemFlag0 & 0x00000001) != 0 );
var g_isShowBWCtrl  =( (g_oemFlag0 & 0x00000002) != 0 );
var g_isShowUPnP    =( (g_oemFlag0 & 0x00000004) != 0 );


//define codec:
var V_JPEG=1000;
var V_MPEG4=1005;
var V_H264=1010;

//initialize language name.
{
  var o='';
  var t=g_s_mui;
  var i=0;
  while (t > 0)
  {
    if (t%2 == 1)
    {
      o+=g_langFullNameList[i]+";";
    }
    t = Math.floor(t/2);
    i++;
  }
  o = o.substring(0,o.length-1);
  g_langNameList = o.split(";");
  
  if (g_langNameList.length == 0 || g_mui < 0 || g_langNameList[0]=="")  g_langName = "en_us";
  else if (g_langNameList.length == 1)  g_langName = g_langNameList[0];
  else g_langName = g_langFullNameList[g_mui];
}

//This is xml http request object for dynamic fetch html
//var xhttp = null;
var THIS_PAGE = GetWebPageName(location.href);

//check brower type, and set show ,hide keyword.
if (window.ActiveXObject)
{
  browser_IE = true;
  keyword_Show = "visible";
  keyword_Hide = "hidden";
}
else if (document.layers)
{
  browser_Netscape = true;
  keyword_Show = "show";
  keyword_Hide = "hide";
}
else
{
  browser_FireFox = true;
  keyword_Show = "visible";
  keyword_Hide = "hidden";
}

//if (IsMpeg4())
//if(g_isSupMpeg4)
//{
//  g_viewXSize = g_mpeg4XSize;
//  g_viewYSize = g_mpeg4YSize;
//}
//else
//{
//  g_viewXSize = g_jpegXSize;
//  g_viewYSize = g_jpegYSize;
//}

//Video Server fix size.
if (IsVS())
{
  g_viewXSize = 320;
  g_viewYSize = (g_isPal) ? 288 : 240;
}
//init the sub menu value
var MENU_ITEM_IMAGE;
var MENU_ITEM_SYSTEM;
var MENU_ITEM_NETWORK;
var MENU_ITEM_APP_SET;
var MENU_ITEM_APP_REC;
var MENU_ITEM_APP_ALARM;

ReloadSubMenu();

function IsMozilla()
{
  return (browser_Netscape || browser_FireFox);
}

function IsInBadLinkList(link)
{
  if (link == null || link == "")
    return false;
  link = link.toLowerCase();
  for(var i=0;i<g_badLinkList.length;i++)
  {
    if (TestRE(link,g_badLinkList[i]))
    {
      return true;
    }
  }
  return false;
}


function TagAX1(id,width,height)
{
  var o='<div id="'+AxRoomID+'" name="'+AxRoomID+'"><OBJECT ID="'+((id == null)?AxID:(AxID+id))+'"';
  o+=' CLASSID="CLSID:'+AxUuid+'"';
  o+=' CODEBASE="/VDControl.CAB#version='+AxVer+'" width='+width+' height='+height+' ></OBJECT></div>';
  //alert(o);
  return o;
}


function ChangeAx2Pic(w,h,forceWait)
{
  var rooms = GES(AxRoomID);
  if (rooms != null)
  {
    if (w == null)
    {
      if (rooms.length > 1 || CONTENT_PAGE != "main.htm")
      {
        w=320;
        h=240;
      }
      else
      {
        w=640;
        h=480;
      }
    }
    var o='<img src="noActiveX.gif" ';
    o+=' width="'+w+'" height="'+h+'"';
    o+=' />';

    for (var i=0;i<rooms.length;i++)
    {
      if (forceWait!=true)
      {
        rooms[i].innerHTML=o;
      }
    }
    
  }
};

function ChangeAx2LimitPic(w,h,forceWait)
{
  var rooms = GES(AxRoomID);
  if (rooms != null)
  {
    if (w == null)
    {
      if (rooms.length > 1 || CONTENT_PAGE != "main.htm")
      {
        w=320;
        h=240;
      }
      else
      {
        w=640;
        h=480;
      }
    }
    var o='<img src="limitfull.gif" ';
    o+=' width="'+w+'" height="'+h+'"';
    o+=' />';

    for (var i=0;i<rooms.length;i++)
    {
      if (forceWait!=true)
      {
        rooms[i].innerHTML=o;
      }
    }
    
  }
};


function GetLiveUrlNoID(ifrmonly,audioEn,sMode)
{
  var o;
 // o = 'http://'+location.host+'/ipcam/'+ (IsMpeg4()?"mpeg4":"mjpeg") + '.cgi?language=ie';
  o = 'http://'+location.host+'/ipcam/';
  //20081219 chirun addand modify for 7313
  if(sMode == 1)
  {
    o+="mjpeg.cgi";
  }
  if (sMode == 2)
  {
    //o+="mpeg4.cgi";
    o+="mjpegcif.cgi";
  }
  if (sMode == 3)
  {
    //o+="mpeg4cif.cgi";
    o+="mpeg4.cgi";
  }
  if (sMode == 4)
  {
    o+="mpeg4cif.cgi";
  }
  
  o+="?language=ie";

  if (ifrmonly == 1)
  {
    o+="&ifrmonly=1";
  }
  if(audioEn == 1)
  {
    o+="&audiostream=1";
  }
  //alert(o);
  return o;
}
//forceWait: do not show the graph message. it will wait the ActiveX be installed.
//mode:99 is for display title
function StartActiveXOneEx(ifrmonly,audioEnable,codec,chid,axid,mode,forceWait)
{
  var obj = (axid == null) ? GE(AxID) : GE(AxID+axid) ;

  if (obj != null)
  {
    try
    {
	  /*
      obj.ViewStop();
      obj.StopReceiver();
      obj.Stop();
      var o = GetLiveUrlNoID(ifrmonly,audioEnable);
      var aud=(audioEnable==1)?1:0;
      obj.IsPlaySound = aud;
      obj.IsPlayAudioForStart = aud;
      if (ifrmonly == 1)
      {
        obj.OnlyIFrame = 1;
      }
      else
      {
        obj.OnlyIFrame = 0;
      }
      
      if (chid != null)
      {
        o+="&ch="+chid;
        obj.CHID = chid;
      }
      //alert(o);
      obj.LiveURL = o;
      if (mode == null)
      {
        mode = 0;
      }
      if (codec != null && codec != 0)
      {  
        obj.VideoCodec = codec;
      }
      obj.SetVideoPreferSize(g_viewXSize,g_viewYSize);
      //obj.IsPlayAudioForStart = 1;
      mode |= 0x10000001;
      obj.UIMode = mode;
      obj.IsAutoReconnect = 0;
      obj.TunnelType = 2;
      obj.IsAutoReconnect = 0; //disable auto reconnect function
      obj.ProfileID = mpMode;
      obj.Start(3); // --> will receive event.
      obj.StartReceiver();
      obj.ViewStart();
	  */
	  g_lockReconnenct = false;
	  obj.Stop();
	  url = location.host.split(":");
      obj.URL = url[0];
      obj.User = "";
      obj.Password = "";
	  //alert(mpMode)
	  if(audioEnable)
	  {
        obj.ReqCommand = '/ipcam/stream.cgi?nowprofileid='+mpMode+'&audiostream=1';
		//obj.IsPlayAudio = 1;
	  }
	  else
	  {
	    obj.ReqCommand = '/ipcam/stream.cgi?nowprofileid='+mpMode+'&audiostream=0';
		//obj.IsPlayAudio = 0;
	  }
      
	  switch(window.location.protocol) 
      {
        case "http:":
          obj.IsHttps = 0;
		  if(g_Port == "" || g_Port == "null" || g_Port == " " || g_Port == null || isNaN(g_Port))
            g_Port = 80;
        break 
        case "https:":
          obj.IsHttps = 1;
		  if(g_Port == "" || g_Port == "null" || g_Port == " " || g_Port == null || isNaN(g_Port))
            g_Port = 443;
        break
        default:
          obj.IsHttps = 0;
		  if(g_Port == "" || g_Port == "null" || g_Port == " " || g_Port == null || isNaN(g_Port))
            g_Port = 80;
        break
      }
	  obj.HttpPort = g_Port;
	  //alert(g_viewXSize+":"+g_viewYSize);
      obj.Width = g_viewXSize;
      obj.Height = g_viewYSize;
      //Status 0 is none, 1 is Privacy Mask , 2 is motion ,3 is Zoom
      obj.Status(mode);
	  var audiotype = 0;
	  if(g_audioType == "G.711")
	    audiotype = 0;
	  if(g_audioType == "G.726")
        audiotype = 1;
		
	  //0:ulaw 1:g726  2:aac
	  obj.AudioType = audiotype;
      //motion
      obj.Motionxblock = 12;
      obj.Motionyblock = 8;
	  obj.MotionBlock = g_motionBlock;
      var status = obj.Start();
	  if(status == 503)
	  {
	    ChangeAx2LimitPic(null,null,false);
		alert(GL("limitfull"));
	  }
	  setTimeout(UnlockReconnenct, 100);
	
    }
    catch (e)
    {
      ChangeAx2Pic(null,null,forceWait);
    }
  }
  else
  {
    ChangeAx2Pic();
  }
};

function UnlockReconnenct()
{
  g_lockReconnenct = false;
};
//id : channel id.
//forceWait: do not show the graph message. it will wait the ActiveX be installed.
//codec: prefer show which codec?
function StartActiveXEx(ifrmonly,audioEnable,codec,chid,axid,mode,forceWait)
{
  var myID = null;
  if (IsVS())
  {
    // id == null mean run all.
    if (chid == null || chid == 0)
    {
      var i;
      for (i=1;i<=g_maxCH;i++)
      {
        StartActiveXOneEx(ifrmonly,audioEnable,codec,i,i,mode,forceWait);
      }
    }
    else
    {
      StartActiveXOneEx(ifrmonly,audioEnable,codec,chid,axid,mode,forceWait);
    }
  }
  else
  {
    //ipcam has no channel
    StartActiveXOneEx(ifrmonly,audioEnable,codec,null,null,mode,forceWait);
  }

};


// get object from id
function GE(name)
{
  return document.getElementById(name);
}
// get obj array from name
function GES(name)
{
  return document.getElementsByName(name);
}
//genernate the "select" html tag
function SelectObject(strName,strOption,intValue,onChange)
{
  DW(SelectObjectNoWrite(strName,strOption,intValue));
}
function SelectObjectNoWrite(strName,strOption,intValue,onChange)
{
  var o='';
  o+='<SELECT NAME="'+strName+'" id="'+strName+'" ';
  if (onChange == null)
  {
    o+='>';
  }
  else
  {
    o+=' onChange="'+onChange+'" >';
  }
  aryOption = strOption.split(';');
  for (var i = 0; i<aryOption.length; i++)
  {
    if(i==intValue)
    {
      o+='<OPTION selected value='+i+'>'+aryOption[i];
    }
    else
    {
      o+='<OPTION value='+i+'>'+aryOption[i];
    }
  }
  o+='</SELECT>';
  return o;
}


//Genernate "select" , the value is number
// include start and end
function CreateSelectNumber(name,start,end,gap,init)
{
  DW(GetSelectNumberHtml(name,start,end,gap,init));
}
//fixNum : the digital fix count.
function GetSelectNumberHtml(name,start,end,gap,init,onChange,onFocus,fixNumC)
{
  var o='';
  o+='<select id="'+name+'" name="'+name+'" class="m1" ';
  if (onChange != null)
  {
    o+=' onchange="'+onChange+'"';
  }
  if (onFocus != null)
  {
    o+=' onfocus="'+onFocus+'"';
  }
  o += '>';
  var i=0;
  for (i=start;i<=end;i+=gap)
  {
    var fixStr = i;
    if(fixNumC != null)
    {
      fixStr = FixNum(i,fixNumC);
    }
    if(i==init)
    {
      o+='<option selected value='+i+'>'+fixStr;
    }
    else
    {
      o+='<option value='+i+'>'+fixStr;
    }
  }
  o+='</select>';
  return o;
}

// return the radio button that be checked.
function GetRadioValue(name)
{
  var value = 0;
  var i;
  var radioObj = GES(name);
  if (radioObj != null)
  {
    for (i=0;i<radioObj.length;i++)
    {
      if (radioObj[i].checked == true)
      {
        value = radioObj[i].value;
        break;
      }
    }
  }
  return value;
}
// if  radio button value equal vv , then check this option.
function SetRadioValue(name,vv)
{
  var i;
  var radioObj = GES(name);
  if (radioObj != null)
  {
    for (i=0;i<radioObj.length;i++)
    {
      if (radioObj[i].value == vv)
      {
        radioObj[i].checked = true;
        break;
      }
    }
  }
}
function IsMpeg4()
{
  return (g_isSupMpeg4);
}


function CreateText(name,size,maxlength,value,isPassword,onChangeFunc)
{
  DW(CreateTextHtml(name,size,maxlength,value,isPassword,onChangeFunc));
}
function CreateTextHtml(name,size,maxlength,value,isPassword,onChangeFunc,onKeyUP)
{
  var type = "text";
  if (isPassword)
  {
    type = "password";
  }
  else if (size == "0")
  {
    type="hidden";
  }
  var o='';
  o+="<input name='"+name+"' id='"+name+"' type='"+type+"' size='"+size+"' maxlength='"+maxlength+"' value='"+value+"'";
  if (onChangeFunc != null)
  {
    o+="onChange='"+onChangeFunc+"' ";
  }
  if (onKeyUP != null)
  {
    o+="onKeyup='"+onKeyUP+"' ";
  }
  o+=">";
  return o;
}



//===================================================
// Validate
//===================================================
//test the reqular expression  , v is value, re is regualar expression
function TestRE(v, re) 
{
	return new RegExp(re).test(v);
}

//check the string is null or blank.
//return ture -> str is not null or ""
//return false -> str is 
function CheckIsNull(str,defMsg,msg,isQuite)
{
  var result = false;
  if (CheckIsNullNoMsg(str))
  {
    if (isQuite != true)
    {
      if (msg != null)
      {
        alert(msg);
      }
      else
      {
        alert(GL("verr_vacuous",{1:defMsg}));
      }
    }
    result = true;
  }
  return result;
}
function CheckIsNullNoMsg(str)
{
  return (str == null || str == "");
}
//===================================================
//check E-Mail
function CheckBadEMail(str,msg,isQuite)
{
  var result = false;
  if (!TestRE(str,'^[-!#$%&\'*+\\./0-9=?A-Z^_`a-z{|}~]+@[-!#$%&\'*+\\/0-9=?A-Z^_`a-z{|}~]+\.[-!#$%&\'*+\\./0-9=?A-Z^_`a-z{|}~]+$'))
  {
    if (isQuite != true)
    {
      if (msg == null)
      {
        alert(GL("verr_email"));
      }
      else
      {
        alert(msg);
      }
    }
    result = true;
  }
  return result;
}
//===================================================
//check the range of number.

//check the range of  number ,include min and max ( min <= x <= max)
//true: the number(str) is not in the range.
function CheckBadNumberRange(str,min,max,defMsg,msg,isQuite,extra)
{
  var result = false;
  var num = parseInt(str);
  var isExtra = (extra != null) ? (num == extra) : (false);
  if (!( num >= min && num <= max) && !isExtra)
  {
    if (isQuite != true)
    {
      if (msg == null)
      {
        if(extra == null)
          alert(GL("verr_bad_num",{1:defMsg,2:min,3:max}));
        else
          alert(GL("verr_bad_num_ex",{1:defMsg,2:min,3:max,4:extra}));
      }
      else
      {
        alert(msg);
      }
    }
    result = true;
  }
  return result;
}
//===================================================
//check the length of the string.
//true: if the length of the string is not in range(minLen <= x <= maxLen)
function CheckBadStrLen(str,minLen,maxLen,defMsg,msg,isQuite)
{
  var result = false;
  if (str == null)
  {
    result = true;
  }
  else
  {
    var len = str.length;
    if (!( len >= minLen && len <= maxLen))
    {
      if (isQuite != true)
      {
        if (msg == null)
        {
          if (minLen != maxLen)
          {
            alert(GL("verr_str_len",{1:defMsg,2:minLen,3:maxLen}));
          }
          else
          {
            alert(GL("verr_str_len2",{1:defMsg,2:minLen}));
          }
        }
        else
        {
          alert(msg);
        }
      }
      result = true;
    }
  }
  return result;
}


//check string , include the non alphabet or digital char.
//true : found non alphabet or digital char, False: all chars are belong alphabet or digital number
//noCheckNum : do not check digital number. it's mean  only check alphabet
//noCheckLower: do not check lower alphabet,
//noCheckLower: do not check upper alphabet,
function CheckBadEnglishAndNumber(str,defMsg,msg,noCheckNum,noCheckLower,noCheckUpper,isQuite)
{
  var result = false;
  if (str == null)
  {
    result = true;
  }
  else
  {
    var i = 0;
    for (i=0;i<str.length;i++)
    {
      var cc = str.charCodeAt(i);
      var checker = true; //true : means baddly.
      if (noCheckNum == null || noCheckNum == false)
      {
        checker = checker && !(cc>=48 && cc<=57);
      }
      if (noCheckLower == null || noCheckLower == false)
      {
        checker = checker && !(cc>=97 && cc<=122);
      }
      if (noCheckUpper == null || noCheckUpper == false)
      {
        checker = checker && !(cc>=65 && cc<=90);
      }
      if (checker)
      {
        if (isQuite != true)
        {
          if (msg == null)
          {
            alert(GL("verr_eng_digital",{1:defMsg}));
          }
          else
          {
            alert(msg);
          }
        }
        result = true;
        break;
      }
    }
  }
  return result;
}


function DW(str)
{
  document.write(str);
}
//===================================================
//Submit the form 
//===================================================
//create the submit button
//CLID: Control List ID
function CreateSubmitButton(CLID,isAsync)
{
  DW(CreateSubmitButton_(CLID,isAsync));
}
function CreateSubmitButton_(CLID,isAsync)
{
  
  var fun = "ValidateCtrlAndSubmit";
  fun += ((CheckIsNullNoMsg(CLID))?"(CTRLARY":("("+CLID) );
  fun += ((CheckIsNullNoMsg(isAsync))?"":(","+isAsync) );
  fun += ")";
  //alert(fun+isAsync);
  
  var o='';
  o+='<input type="button" id="smbtn_'+((CLID==null)?"":CLID)+'" value="'+GL("submit")+'" class="m1" onClick="'+fun+'">';
  return o;

}


function SendHttp(url,isAsync,callBack)
{
  isAsync = new Boolean(isAsync);
  g_SubmitHttp = null;
  g_SubmitHttp = InitXHttp();
  if (callBack != null)
  {
    g_SubmitHttp.onreadystatechange = callBack;
  }
  else
  {
    g_SubmitHttp.onreadystatechange = OnSubmitReadyStateProcess;
  }
  
  try
  {
    g_SubmitHttp.open("GET", url, isAsync);
    g_SubmitHttp.setRequestHeader("If-Modified-Since","0");
    g_SubmitHttp.send(null);
    WS(GL("sending_"));
  }catch(e){};
}


function OnSubmitReadyStateProcess()
{
  if (g_SubmitHttp.readyState==4)
  {
    if (g_SubmitHttp.status != 200)
    {
      alert(GL("err_submit_fail"));
      g_httpOK = false;
      WS(GL("fail_"));
    }
    else
    {
      g_httpOK = true;
      WS(GL("ok_"));
    }
  }
}

function ShowObjVar(obj)
{
  var ssss;
  for (i in obj) ssss += i+"="+obj[i]+", ";
  document.write("<h1>"+ssss+"</h1>");
}


function IsChecked(name)
{
  var obj = GE(name);
  var result = false;
  if (obj != null)
  {
    result = obj.checked;
  }
  return result;
}
function SetChecked(name,isChk)
{
  var obj = GE(name);
  if (obj != null)
  {
    obj.checked = isChk;
  }
}
function Bool2Int(data)
{
  return (data) ? 1 : 0;
}
//===================================================
// Get the value form the object.
//===================================================
//return obj.value
function GetValue(name)
{
  var result = '';
  var obj = GE(name);
  if (obj != null)
  {
    if (obj.type.indexOf("select") >= 0)
    {
      result = obj.options[obj.selectedIndex].value;
    }
    else
    {
      result = obj.value;
    }
  }
  else
  {
    alert(GL("verr_miss_obj",{1:name}));
  }
  return result;
}

//===================================================
// set the value of the object.
//===================================================
function SetValue(name,value)
{
  var obj = GE(name);
  if (obj != null)
  {
    if (obj.type.indexOf("select") >= 0)
    {
      for (var i=0;i<obj.options.length;i++)
      {
        if (obj.options[i].value==value)
        {
          obj.selectedIndex = i;
          break;
        }
      }
    }
    else
    {
      obj.value = value;
    }
  }
  else
  {
    alert(GL("verr_set_value",{1:name}));
  }
}

function GetDeviceTitleStr(extName)
{
  //var devName = g_deviceName;
  var devName = g_titleName;

  if (extName == null)
  {
    return GetTitleTag(devName);
  }
  else
  {
    return GetTitleTag(extName);
  }
}
function GetTitleTag(titleStr)
{
  return ("<title>"+titleStr+"</title>");
}

// if exist otherName then the document title will be "otherName" and the "extTitle" will be nonsense 
//if the "otherName" = null, but the "extTitle" exist, then the document title will be "<%devicename%> + extTitle"
// If both of them are equals null, then the document title will be "<%devicename%>" , and  the "<%devicename%>" default value is "LAN Camera"
function WriteHtmlHead(extTitle,otherName,onLoadFunc,onUnLoadFunc,onResizeFunc,refreshTime)
{
  var o='';
  o+=GetHtmlHeaderNoBannerStr(extTitle,otherName,onLoadFunc,onUnLoadFunc,onResizeFunc,refreshTime);
  // 20090113 Netpool Add for D-Link Style
  o+='<table border="0" cellspacing="1" cellpadding="2" align="center" bgcolor="#FFFFFF" width="800" >';
  o+='<tr><td height="25" ><img src="D-Link.gif" border=0></td></tr>';
  o+='<tr><td id="mainMenuBar" >'+GetMenuStr()+'</td></tr>';
  o+='<tr><td id="DLinkToolBar" >'+GetDLinkToolStr()+'</td></tr>';
  //o+='<tr><td colSpan=3>';
  //o+='<img height=61 src="'+g_brandName+'.jpg" width=770 useMap=#Map border=0></td></tr>';
  //o+=GetLogoHtml();
  //o+=GetViewCHHtml();
  //fix ActiveX can't install. add an <OBJECT> on the index page.
  //20061220 To avoid many trouble, so force user install activex.
  //if (g_useActiveX == 1 || IsMpeg4() || IsVS() )
  //o+='<OBJECT CLASSID="CLSID:'+AxUuid+'" CODEBASE="/VDControl.CAB#version='+AxVer+'" width=0 height=0 ></OBJECT>';
  //o+='<img id="arrowImg" style="position:absolute;left:-600px;z-index:5;cursor: pointer;" src="arrow.gif" onClick="ClickArrow(event)" onMouseMove="MoveOnArrow(event)" onMouseOut="WS()" border=0 />';
  // 20081204 chirun modify for 7313 355 d-link 
  // modify o+='<img height=61 src="nobrand.jpg" width=770 useMap=#Map border=0></td></tr>';
  //o+='<div id ="imgbrand" ><img height=61 src="nobrand.jpg" width=770 useMap=#Map border=0></div>';
  // 20081204 chirun modify for 7313 355 d-link 
  // del o+='<div id ="imgbrand" style="background-image: url(newbrand.gif); width:770; height:61; "  useMap=#Map border=0> </div></td></tr>';
  //o+='<tr><td width=12.5 valign="top" background="ie_02.jpg" height="492">&nbsp;</td>';
  //o+='<div align=center>';
  //o+='<td width=745 align=center>';
  //o+='<td valign="top" align=center ><table ><tr>';
  o+='<tr><td><div id="WebContent" >';
  DW(o);
};

function GetHtmlHeaderNoBannerStr(extTitle,otherName,onLoadFunc,onUnLoadFunc,onResizeFunc,refreshTime)
{
  var o='';
  o+=GetHtmlHeaderNoBodyStr(extTitle,otherName,refreshTime);
  // 20090112 Netpool Modified for D-Link Style
  o+='<body topmargin="1" leftmargin="0" rightmargin="0" bgcolor="#757575" ';
  if(onLoadFunc != null)
  {
    o+='onLoad="'+onLoadFunc+'()" ';
  }
  if(onUnLoadFunc != null)
  {
    o+='onUnLoad="'+onUnLoadFunc+'()" ';
  }
  if(onResizeFunc != null)
  {
    o+='onResize="'+onResizeFunc+'()" ';
  }
  // 20090115 Netpool Modified for D-Link Style
  o+='>';
  return o;
};

function GetHtmlHeaderNoBodyStr(extTitle,otherName,refreshTime)
{
  var o='';
  //o+='<html ><head>';
  if (otherName != null)
  {
    o+=GetTitleTag(otherName);
  }
  else if (extTitle != null)
  {
    o+=GetDeviceTitleStr(extTitle);
  }
  else
  {
    o+=GetDeviceTitleStr();
  }
  if (refreshTime != null)
  {
    o+='<meta HTTP-EQUIV="Refresh" CONTENT="'+refreshTime+'">';
  }
  //o+='<meta http-equiv=Content-Type content="text/html; charset=UTF-8">';
  o+='<meta HTTP-EQUIV="CACHE-CONTROL" CONTENT="NO-CACHE">';
  //20070518 it seems css not really need for different lang.
  //o+='<link href="lc_'+g_langName+'.css" rel="stylesheet" type="text/css">';
  o+='<link href="lc_en_us.css" rel="stylesheet" type="text/css">';
  o+='</head>';
  return o;
}

function IsViewer()
{
  return (g_isAuthorityChange && g_socketAuthority > 1);
}
//get link string, it will depends on authority.
//20060801 fix link to use XMLHttpRequest to fetch web page.
function GetLinkStr(link,key,type)
{
  var o='';
  if (IsViewer())
  {
    o+=GL(key);
  }
  else
  {
    // 20090206 Netpool Modified for D-Link Style
    o+=GetDLinkContentLink(key,link,type);
  }
  return o;
}

function MainMenuDown(obj,key)
{
  obj.className = "menuLinkDOWN";
  //WS();
}

function MainMenuIn(obj,key,isMM)
{
  obj.className = "menuLinkON";
  //var k = obj.innerHTML.toLowerCase().replace(new RegExp(" ","g"),"_").replace(new RegExp("/","g"),"_");
  if (isMM != false)
    WS( GL(key+"_comment") );
}

function MainMenuOut(obj,key)
{
  obj.className = "menuLink";
  //WS();
}

function GetFirstOKLink(menuAry)
{
  for (var i=2;i<menuAry.length;i+=3)
  {
    if (parseInt(menuAry[i]) == 1)
      return menuAry[i-1];
  }
  return null;
}

//calculate bottom menu
var totalMenuItem = 0;
// 20090116 Netpool Modified for D-Link Style
function GetMenuStr()
{
  var o='';
  o+='<table id="DLinkMainTable" border="0" cellSpacing="0" cellPadding="2" >';
  o+='<tr class="topnav_container" >';
  o+='<td class="td1" ><img src="dcs3110.gif" width="125" height="25" ></td>';
  o+='<td class="td1" id="MI_0">'+GetLinkStr("index.htm","home","home")+'</td>';
//  o+='<td width="115" height="24">';
//  if (IsViewer())
//    o+='&nbsp;';
//  else
//  {
//    o+='<a href="version.htm" target=_blank>';
//    o+='<img border="0" src="version.gif" width="10" height="10"></a>';
//  }
  o+='<td class="td1" id="MI_1">'+GetLinkStr("image.htm","image","image")+'</td>';
  o+='<td class="td1" id="MI_2">'+GetLinkStr("network.htm","network","network")+'</td>';
  o+='<td class="td1" id="MI_3">'+GetLinkStr("system.htm","system","system")+'</td>';
  totalMenuItem = 4;
  var tmpLink = GetFirstOKLink(MENU_ITEM_APP_SET);
  if (tmpLink == null) tmpLink = GetFirstOKLink(MENU_ITEM_APP_REC);
  if (tmpLink == null) tmpLink = GetFirstOKLink(MENU_ITEM_APP_ALARM);
  if (tmpLink != null)
  {
    o+='<td class="td1" id="MI_4">'+GetLinkStr("application.htm","application","application")+'</td>';
    totalMenuItem++;
  }
  //add no card check.
  if (g_defaultStorage != NO_STORAGE && g_defaultStorage<2)
  {
    o+='<td class="td1" id="MI_5">';
    if(g_defaultStorage == "1")
    {
      //if(g_SDInsert == "1")
      if(false)
        o+=GetLinkStr(g_cardGetLink,"sd_card","card");
      else
        o+=GL("sd_card");
    }
    else
    {
      if(g_CFInsert == "1")
        o+=GetLinkStr(g_cardGetLink,"cf_card","card");
      else
        o+=GL("cf_card");
    }
    o+='</td>';
    totalMenuItem++;
  }
//  if (g_isSupportptzpage && !g_isShowPtzCtrl)
//  {
//    o+='<td class="td1" id="MI_6">'+GetLinkStr("ptz.htm","pan_tilt","pop")+'</td>';
//    totalMenuItem++;
//  }
  o+='<td class="td1" id="MI_7">'+GetLinkStr( ("help.htm"),"help","help")+'</td>';
  totalMenuItem++;
  o+='</tr></table>';
  return o;
};

function PopupPTZ(URL)
{
  //return PopupPage(URL,"PTZ",80,0,575,125);
  return PopupPage(URL,"PTZ",80,0,455,125);
}
function PopupPage(URL,id,x,y,w,h)
{
  var windowProps = "location=no,scrollbars=no,menubars=no,toolbars=no,resizable=no,left="+x+",top="+y+",width="+w+",height="+h;
  return WindowOpen(URL,id,windowProps);
}
function WindowOpen(URL,id,props)
{
  var popup = window.open(URL,id,props);
  try
  {
    popup.focus();
  }catch(e){};
}

function GetMapLinkStr()
{
  var o='';
  o+='<MAP name=Map>';
  //if(g_brandUrl != null && !CheckIsNullNoMsg(g_brandUrl) && g_brandUrl.indexOf("%") < 0 && g_brandUrl != "(null)")
  //{
	//  o+="<AREA shape=RECT coords='27, 10, 205, 50' href='"+g_brandUrl+"' target=_blank name='brand'>";
  //}
	o+='<AREA shape=RECT coords="556, 42, 636, 58" href="index.htm" name="index">';
  if (!IsViewer())
  {
    o+='<AREA shape=RECT coords="635, 42, 691, 58" href="help.htm" target=_blank name="help">';
  }
  //o+='<AREA shape=RECT coords="692, 42, 757, 58" href="http://'+location.host+'logout.htm" name="logout">';
  o+='<AREA shape=RECT coords="692, 42, 757, 58" href="logout.htm" name="logout">';
  o+='</MAP>';
  return o;
}

function WriteBottom(lastctx)
{
  var o='';
  //o+='</td>';
  //o+='</div>';
  //o+='</tr></table></td>';
  o+='</div></td></tr></table>';
  //o+='<td width=14 valign="top" background="ie_04.jpg" height="492">&nbsp;</td></tr>';
  //o+='<tr><td colSpan=3 id="mainMenuBar">';
  //o+=GetMenuStr();
  //o+='</td></tr>'
  // 20090119 Netpool Modified for D-Link Style
  //o+='<tr><td background="address.jpg">';
  //o+='<p align="right" class="m2"><span id="devTitleLayer">'+g_titleName+'</span>&nbsp&nbsp&nbsp&nbsp;</p>';
  //o+='</td></tr></table>';
  // 20090119 Netpool Modified for D-Link Style
  o+='<iframe name="retframe" frameborder="0" style="display:none;"></iframe>';
  o+='<div class="div1" align="center">Copyright &copy; 2008 D-Link Corporation/D-Link Systems, Inc. All rights reserved.</div><br>';
  o+=GetMapLinkStr();
  if (lastctx != null)
  {
    o+=lastctx;
  }
  o+='</body>';
  DW(o);
  //20071109 Luther add , fix text to center.
  //ResizeMenuItems();
}

function ResizeMenuItems()
{
  //resize
  var tmpW = 660 / totalMenuItem;
  for(i=0;i<9;i++)
  {
    try
    {
      var obj = GE("MI_"+i);
      if (obj != null)
      {
        obj.width = tmpW;
      }
    }
    catch(e){};
  }
}

function ReloadSubMenu()
{
  MENU_ITEM_IMAGE = new Array(
    'image','img.htm',1,
    //'image_h264','img_h264.htm',g_is264,
    'fine_tune','imgtune.htm',1,

    'privacy_mask','imgmask.htm',g_s_maskarea,
	//20081224 chirun add for 7313
    'image_daynight','imgdaynight.htm',g_s_daynight,
	"checkout","checkout.htm",1
  );
  
  //Note: do not mix the order, the img.htm will direct acces
  //MENU_ITEM_SYSTEM[23]
  MENU_ITEM_SYSTEM = new Array(
    'date_and_time','sdt.htm',1,
    'timestamp','sts.htm',(g_supportTStamp>0)?1:0,
    'users','suser.htm',1,
    'digital_io','sdigital.htm',1,
    'audio_mechanism','saudio.htm',(g_isSupportAudio)?1:0,
    //'rs232_set','srs.htm?:232',(g_isSupportRS232)?1:0,
    'rs485_set','srs.htm',(g_isSupportRS485)?1:0,
    //'sequence','sseq.htm',(g_isSupportSeq)?1:0,
    'update','c_update.htm',(g_isShowUpdate)?1:0,
    'event','aevt.htm',1,
	"checkout","checkout.htm",1
  );

  
  MENU_ITEM_NETWORK = new Array(
    "network","net.htm",1,
    "nftp","nftp.htm",g_serviceFtpClient,
    "nsmtp","nsmtp.htm",g_serviceSmtpClient,
    "sntp","nsntp.htm",g_serviceSNTPClient,
	
	// 20081128 chirun add for test  start
	"virtual_server","nvirtualserver.htm",g_serviceVirtualSClient,
	// 20081128 chirun add for test  end
	
    "ddns","nddns.htm",g_serviceDDNSClient,
    "wireless","nwireless.htm",(g_isSupWireless)?1:0,
    "pppoe","npppoe.htm",g_servicePPPoE,
    //"ftp_host","nftphost.htm",g_AdvMode,
    "upnp","c_nupnp.htm",(g_isShowUPnP)?1:0,
    //"mcenter","mcenter.htm",g_AdvMode,
    //"dhcp_server","ndhcpsvr.htm",g_AdvMode,
    //"ip_filter","nipfilter.htm",((!g_X)&&g_isSupportIPFilter)?1:0,
    "ip_filter","nipfilter.htm",(g_isSupportIPFilter)?1:0,
    "bwc_traffic","c_bwcntl.htm",(g_isShowBWCtrl)?1:0,
	"checkout","checkout.htm",1
  );

  
  MENU_ITEM_APP_SET = new Array(
  
    // 20081203 chirun del for 7313 355 d-link
    //del "video_file","setvid.htm",1,
	
	"ftp","setftp.htm",g_serviceFtpClient,
	// 20081128 chirun add for language  start
	"langx","c_lang.htm",(g_langNameList.length > 1)?1:0,
	// 20081128 chirun add for language  end
	
    
    "app_sd_card","setcard.htm",(parseInt(g_defaultStorage)==1)?1:0,
    //"app_cf_card","setcard.htm",(parseInt(g_defaultStorage)==0)?1:0,
    "smtp","setsmtp.htm",g_serviceSmtpClient,
	"checkout","checkout.htm",1
    //"langx","c_lang.htm",((!g_X)&&g_langNameList.length > 1)?1:0
    //"langx","c_lang.htm",(g_langNameList.length > 1)?1:0
  );

  MENU_ITEM_APP_REC = new Array(
    "enable_rec","renable.htm",((!ISNOSTORE)||(g_serviceFtpClient==1))?1:0,
    "schedule","rsch.htm",((!ISNOSTORE)||(g_serviceFtpClient==1))?1:0,
	"checkout","checkout.htm",1
  );

  MENU_ITEM_APP_ALARM = new Array(
    "enable_alarm","aenable.htm",1,
    "motion_detect","motion.htm",(g_isSupMotion)?1:0,
	"checkout","checkout.htm",1
  );
  
  
  
  MENU_ITEM_STATUS = new Array(
    "status_menu_decive","status_info.htm",1,
	"status_menu_log","status_log.htm",1,
	"checkout","checkout.htm",1
  );
}
function ReloadMainMenu()
{
  GE("mainMenuBar").innerHTML = GetMenuStr();
  //20071109 Luther add , fix text to center.
  //ResizeMenuItems();
}

function StopActiveX()
{
  var obj = GE(AxID);
  if (obj != null)
  {
    try
    {
      obj.ForceStop();
    }
    catch (e)
    {
    }
  }
  if (IsVS())
  {
    var i;

    for (i=1;i<=g_maxCH;i++)
    {
      obj = GE(AxID+i);
      if (obj != null)
      {
        try
        {
          obj.ForceStop();
        }
        catch (e)
        {
        }
      }
    }
  }
  
}


function CheckHex(n,isQuite)
{
  var i,strTemp;
  strTemp="0123456789ABCDEF";
  if ( n.length== 0)
  {
    if (isQuite != true) alert(GL("verr_vacuous",{1:"hex"}));
    return false;
  }
  n = n.toUpperCase();
  for (i=0;i<n.length;i++)
  {
    if (strTemp.indexOf(n.charAt(i)) == -1)
    {
      if (isQuite != true) alert(GL("verr_not_hex"));
      return false;
    }
  }
  return true;
}
function InitXHttp()
{
  var xhttp = null;
  if (IsMozilla())
  { 
    xhttp = new XMLHttpRequest(); 
    if(xhttp.overrideMimeType)
    { 
      xhttp.overrideMimeType('text/xml'); 
    } 
  }
  else if (browser_IE) 
  {
    try
    {
      xhttp = new ActiveXObject("Msxml2.XMLHTTP");
      //xhttp = new ActiveXObject("Microsoft.XMLHTTP");
    }
    catch (e)
    { 
      try
      { 
        xhttp = new ActiveXObject("Microsoft.XMLHTTP"); 
      }
      catch (e){};
    }
  } 
  return xhttp;
}
function GetWebPageName(url)
{
  var ss = url.split("/");
  return ss[ss.length-1];
}
function CreateSystemMenuItem(key,menuUrl,isON)
{
  if (isON == null) isON = 1;
  if (isON != 1) return '';
  //20081225 chirun del for 7313
  //del if ((key == "ftp" || key == "smtp") && IsMpeg4()) return '';

  var o='';
  o+='<tr>';
  //srs.htm
  var isTitle = false;
  if ( menuUrl.indexOf("srs.htm")==0 )
  {
    isTitle = (menuUrl == CONTENT_PAGE);
  }
  else if ( CONTENT_PAGE.indexOf(menuUrl) == 0 )
  {
    isTitle = true;
  }
  if (isTitle)
  {
    o+='<td width="155" height="22" class="sel" style="border-bottom:1px solid #FFFFFF;" >'+GL(key)+'</td>';
  }
  else
  {
    o+=GetMenuItemLink(key,menuUrl);
  }
  o+='</tr>';
  return o;
}
function GetMenuItemLink(key,url,type)
{
  var o="";
  // 20090204 Netpool Modified for D-Link
  o+='<td width="128" height="22" bgcolor="#434343" class="menu" style="border-bottom:1px solid #FFFFFF;" onMouseOver="DLinkMainMenuIn(this,\''+key+'\')" onMouseOut="DLinkMainMenuOut(this,\''+key+'\')" onMouseDown="DLinkMainMenuDown(this,\''+key+'\')" onClick="';
  if (type == "pop")
  {
    o+= 'PopupPTZ(\''+url+'\');';
  }
  else if (type == "card")
  {
    o+= 'WindowOpen(\''+url+'\',\'CARD\',\'location=yes,directorybuttons=no,scrollbars=yes,resizable=yes,menubar=yes,toolbar=yes\');';
  }
  else
  {
    o+= 'ChangeContent(\''+url+'\');';
  }
  o+='" >'+GL(key)+'</td>';
  return o;
}
function GetContentLink(key,url,type)
{
  var o="";
  // 20090116 Netpool Modified for D-Link Style
  o+='<span class="menuLink" onMouseOver="MainMenuIn(this,\''+key+'\')" onMouseOut="MainMenuOut(this,\''+key+'\')" onMouseDown="MainMenuDown(this,\''+key+'\')" onClick="';
  if (type == "pop")
  {
    o+= 'PopupPTZ(\''+url+'\');';
  }
  else if (type == "card")
  {
    o+= 'WindowOpen(\''+url+'\',\'CARD\',\'location=yes,directorybuttons=no,scrollbars=yes,resizable=yes,menubar=yes,toolbar=yes\');';
  }
  // 20090116 Netpool Modified for D-Link Style
  else if(type == "home" || type == "help" || type =="image")
  {
    o+='DLinkGoHref(\''+url+'\')';
  }
  else
  {
    o+= 'ChangeContent(\''+url+'\');';
  }
  o+='" >'+GL(key)+'</span>';
  return o;
}

function CreateSystemMenu(extMenu)
{
  DW(CommonGetMenuStr(MENU_ITEM_SYSTEM, extMenu));
}

function CreateImageMenu(extMenu)
{
  DW(CommonGetMenuStr(MENU_ITEM_IMAGE, extMenu));
}

function GetTagAX1AndFixSize(id)
{
  var o='';
  //20070613 Luther fix 1280*960
//  if (g_viewXSize>700 && g_viewYSize==240)
//  {
//    o+=TagAX1(id,g_viewXSize,480);
//  }
//  else if (g_viewXSize>700 && g_viewYSize==288)
//  {
//    o+=TagAX1(id,g_viewXSize,576);
//  }
//  else if (g_viewXSize==640 && g_viewYSize==240)
//  {
//    o+=TagAX1(id,640,480);
//  }
  //20071024 Luther add, use common way
  if (g_viewXSize>=640 && g_viewYSize<300)
  {
    o+=TagAX1(id,g_viewXSize,g_viewYSize*2);
  }
  else
  {
    var w = g_viewXSize;
    var h = g_viewYSize;
    if (w > 720)
    {
      h = (720 * h) / w;
      w = 720;
    }
    o+=TagAX1(id,w,h);
  }
  return o;
}
function GetHalfViewSizeX()
{

  //20070613 Luther fix 1280 * 960
//  if (g_viewXSize > 400)
//    //return g_viewXSize / 2;
//    return 360;
//  else
//    return g_viewXSize;
  //20070920 for fix radio
  var tmp = g_viewXSize;
  while (true)
  {
    if (tmp > 360)
    {
      tmp /= 2;
    }
    else
    {
      break;
    }
  }
  return tmp;
}
function GetHalfViewSizeY()
{
  //20070613 Luther fix 1280 * 960
//  if (g_viewYSize > 300)
//    //return g_viewYSize / 2;
//    return 240;
//  else
//    return g_viewYSize;
  //20070920 for fix radio
  var tmp = g_viewYSize;
  while (true)
  {
    if (tmp > 288)
    {
      tmp /= 2;
    }
    else
    {
      break;
    }
  }
  return tmp;
}

// return the fix length string, if the length is too short , it will add the insChar.
//type:,0 insert insChar to left, the string will align right,
//type: 1 align left
//type:2 middle
function GetFixLenStr(str,len,insChar,type)
{
  var o="";
  str = ""+str; //make sure it is string
  var count = len - str.length;
  var lc=rc=0;
  if (type==1)
  {
    rc = count;
  }
  else if (type==2)
  {
    lc = count / 2;
    rc = count - lc;
  }
  else
  {
    lc = count;
  }
  if (lc > 0)
  {
    var i=0;
    for (i=0;i<lc;i++)
    {
      o+=insChar;
    }
  }
  o+=str;
  if (rc > 0)
  {
    var i=0;
    for (i=0;i<rc;i++)
    {
      o+=insChar;
    }
  }
  return o;
}

function DisableObject(name,isDisable)
{
  var obj = GE(name);
  if (obj != null)
  {
    obj.disabled = isDisable;
  }
  try
  {
    SetCIA(name, !isDisable);
    //CTRLARY[name].active = !isDisable;
  }catch (e){};

  var objs = GES(name);
  if (objs != null)
  {
    for (var i=0;i<objs.length;i++)
    {
      objs[i].disabled = isDisable;
    }
  }
}
function DisableObjs(list,isDisable)
{
  for (var i=0;i<list.length;i++)
  {
    DisableObject(list[i],isDisable);
  }
}

//get init submit string
//return the  submit POSTDATA init data.
function GetIS()
{
  var o='GET /'+THIS_PAGE+'?';
  o+="language=ie";
  return o;
}

//set the object visible
function SetVisible(name,isVisible)
{
  var obj = GE(name);
  if (obj != null)
  {
    obj.style.visibility = (isVisible) ? keyword_Show : keyword_Hide;
  }
}

//change  size  to Bytes,MB,GB ex: 1035 = 1.02 KB
//fixDotSize decimal point postion , default is 2
function GetCapacityString(size,fixDotSize)
{
  var BASEUNIT = 1024;
  var capStr = '';
  if (fixDotSize == null || fixDotSize < 0)
  {
    fixDotSize = 2;
  }
  var total = parseFloat(size);
  if (total < BASEUNIT)
  {
    capStr = total +' Bytes';
  }
  else
  {
    total /= BASEUNIT;
    total = total.toFixed(fixDotSize);
    if (total == parseFloat(total))
    {
      total = parseFloat(total);
    }
    if (total < BASEUNIT)
    {
      capStr = total+' KB';
    }
    else
    {
      total /= BASEUNIT;
      total = total.toFixed(fixDotSize);
      if (total == parseFloat(total))
      {
        total = parseFloat(total);
      }
      if (total < BASEUNIT)
      {
        capStr = total+' MB';
      }
      else
      {
        total /= BASEUNIT;
        total = total.toFixed(fixDotSize);
        if (total == parseFloat(total))
        {
          total = parseFloat(total);
        }
        capStr = total+' GB';
      }
    }
  }
  return capStr;
}

//input data object and format id isDayLight ,return time string.
//timeFormat : 
// 0-> YY-MM-DD
// 1-> MM-DD-YY
// 2-> DD-MM-YY
function GiveMeDateTimeString(dateObj,timeFormat,isDayLight)
{
  return ( (GiveMeDateString(dateObj,timeFormat,isDayLight)) + " " + (GiveMeTimeString(dateObj)));
}
function GiveMeDateString(dateObj,timeFormat,isDayLight)
{
  var y,M,d;
  y=FixNum(dateObj.getYear(),2);
  M=FixNum(dateObj.getMonth()+1,2);
  d=FixNum(dateObj.getDate(),2);

  var o = "";
  if (isDayLight == 1)
    o += "(+) ";

  if (timeFormat==1)
  {
    o += M +"/"+ d +"/"+ y ;
  }
  else if (timeFormat == 2)
  {
    o += d +"/"+ M +"/"+ y ;
  }
  else
  {
    o += y +"/"+ M +"/"+ d ;
  }
  return o;
}

function GiveMeTimeString(dateObj)
{
  var h,m,s;
  h=FixNum(dateObj.getHours(),2);
  m=FixNum(dateObj.getMinutes(),2);
  s=FixNum(dateObj.getSeconds(),2);
  return (h +":"+ m +":"+ s);
}

function FixNum(str,len)
{
  return FixLen(str,len,"0");
}
function FixLen(str,len,c)
{
  var ins="";
  var i;
  for (i=0;i<len-str.toString().length;i++)
  {
    ins+=c;
  }
  return ins+str; 
}

function CommonGetMenuStr(items,extMenu)
{
  var o='';
  // 20090202 Netpool Modified for D-Link Style
  o+='<table class="left_table" align="center" border="0" cellpadding="2" cellspacing="0" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >';
  var i;
  for (i=0;i<items.length;i+=3)
  {
    o+=CreateSystemMenuItem(items[i],items[i+1],items[i+2]);
  }
  if (extMenu != null)
  {
    for (i=0;i<extMenu.length/2;i++)
    {
      o+=CreateSystemMenuItem(extMenu[i*2],extMenu[(i*2)+1]);
    }
  }
  o+='</table>';
  return o;
}

function GetMenuNetworkStr(extMenu)
{
  return CommonGetMenuStr(MENU_ITEM_NETWORK, extMenu );
}


function GetMenuAppStr()
{
  //APP has three type: setting, record, alarm
  var o='';
  // 20090206 Netpool Modified for D-Link Style
  o+='<table class="left_table" align="center" border="0" cellpadding="2" cellspacing="0" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >';
  //o+='<table border="0" align="right" valign="top" >';
  o+=GetMenuAppSubStr(MENU_ITEM_APP_SET,GL("setting"));
  o+=GetMenuAppSubStr(MENU_ITEM_APP_REC,GL("record"));
  o+=GetMenuAppSubStr(MENU_ITEM_APP_ALARM,GL("alarm"));
  o+='</table>';
  return o;
}
function GetMenuAppSubStr(ITEMS,subName)
{
  if (GetFirstOKLink(ITEMS) == null) return "";
  var o='';
  // 20090206 Netpool Del for D-Link Style
  //o+='<tr class="mm1"><td height="20" colspan="2" >'+subName+'</td></tr>';
  var i;
  for (i=0;i<ITEMS.length;i+=3)
  {
    o+=CreateSystemMenuItem(ITEMS[i],ITEMS[i+1],ITEMS[i+2]);
  }
  return o;
}

function ChangeImageLeftVideo(useAX)
{
  GE("imgAX").innerHTML = GetImageLeftVideo(useAX);
}
function GetImageLeftVideo(useAX)
{
  var o="";
  if (IsMpeg4() || useAX == 1 || useAX == true)
  {
    if (IsVS())
    {
      o = TagAX1(1,GetHalfViewSizeX(),GetHalfViewSizeY());
    }
    else
    {
      o = TagAX1(null,GetHalfViewSizeX(),GetHalfViewSizeY());
    }
  }
  else
  {
    //o= "<img id='sampleimg' name='sampleimg' src='/dms' width=352 height=240 border=2>";
    o+='<table border=3 bordercolor="#99CCFF"><tr><td>';
//alert(imgFetcher.GetDmsImgStr());    
    o+= imgFetcher.GetDmsImgStr(GetHalfViewSizeX(),GetHalfViewSizeY());
    o+='</td></tr></table>';
  }
  return o;
}
//vType is user define variable 0 ,store the  mjpeg mode , display control is  ActiveX or JavaApplet
//isNoShowWaitImg , is in changing don't show the waitting img? 
function WriteImageLeftSide(vType,isNoShowWaitImg)
{
  CreateImageMenu();
//  DW('</div><center><div id="imgAX">');
//  if (isNoShowWaitImg)
//    DW(GetImageLeftVideo(vType));
//  else
//    DW("<img id='sampleimg' name='sampleimg' src='/Loading.gif' width=352 height=240 border=2>");
}

//get Select Object ,be select text not  index
function GetSelectText(name)
{
  var oo="";
  var obj = GE(name);
  if (obj != null)
  {
    oo = obj.options[obj.selectedIndex].text;
  }
  return oo;
}


function CheckBadFqdnLen(str,msg)
{
  var result = false;
  if (str.length > v_maxFqdnLen)
  {
    alert(msg);
    result = true;
  }
  return result;
}

//===========================================
// I18N, 
//===========================================
// GL : GetLanguage
// name: is the key of the i18n string, 
function GL(name, va)
{
  name = LangKeyPrefix + name;
  var v = (typeof(LAry[name]) == "undefined") ? GetDefaultStr(name) : LAry[name];
//alert(name);
  if (va) 
  {
    for (n in va)
      v = this.ReplaceVar(v, n, va[n]);
  }
  return v;
}

//string replace
// the {$xxx} will be treat a variable, and replace it.
// h: the origional string, r:replace key(no  $ sign.),v: replace value.
function ReplaceVar(h, r, v) 
{
//debug
//alert("h="+h+"r="+r+"v="+v);
//alert(h.replace(new RegExp('{\\\$' + r + '}', 'g'), v));
  return h.replace(new RegExp('{\\\$' + r + '}', 'g'), v);
}


function GetDefaultStr(v)
{
  v = v.substring(LangKeyPrefix.length,v.length);
  return v.replace(new RegExp("_","g")," ");
}

function AddLangs(ar) 
{
  for (var key in ar)
  {
    if (typeof(ar[key]) == 'function')
      continue;
//debug
//alert((key.indexOf(LangKeyPrefix) == -1 ? LangKeyPrefix : '') + key);
//alert(ar[key]);
    LAry[(key.indexOf(LangKeyPrefix) == -1 ? LangKeyPrefix : '') + key] = ar[key];
  }
}

//replace content key word to I18N .
function I18NHtml(src)
{
  var len = LangKeyPrefix.length;
  for (var key in LAry)
  {
    //alert(key.substring(len,key.lenght) +" : "+LAry[key]);
    src = ReplaceVar(src,key.substring(len,key.length),LAry[key]);
  }
  //alert(src);
  return src;
}
//===========================================
// about UI setting
//===========================================
//AddLightBtn : add a light button
//GetLight : get the light status. 0:off 1:on
//SwitchLight : switch the light status, on -> off , or off -> on
//SetLightPos : set the light position.

//add a graphic buttion, can display on-off,
//it will put the image in <div> tag.
//id : the light id
//onChangeFun is a function name, ex: turnOn(), if you want to add args, 
//  use \"\",  trunOn(\"1000\")
function AddLightBtn(id,left,top,width,height,onImg,offImg,onChangeFunc,describe)
{
  LightAry["LIGHT_"+id] = new LightInfo(id,onImg,offImg,onChangeFunc);

  var o='';
  //var o="";
  //o+="<div id='LIGHT_"+id+"' style='position:absolute; left:"+left+"px; top:"+top+"px; width:"+width+"px; height:"+height+"px; z-index:10'><img id='LIGHT_PIC_"+id+"' src='"+onImg+"' onClick='SwitchLight(\""+id+"\");' lowsrc='"+offImg+"' sv='1' chg='"+onChangeFun+"' style='cursor:pointer ' alt='"+describe+"'></div>";
  o+='<img name="LIGHT_PIC_'+id+'" id="LIGHT_PIC_'+id+'" src="'+offImg+'" title="'+describe+'" alt="'+describe+'" width="'+width+'" height="'+height+'" >';
  //o+="<div id='LIGHT_"+id+"' style='position:absolute; left:"+left+"px; top:"+top+"px; width:"+width+"px; height:"+height+"px; z-index:10'><img id='LIGHT_PIC_"+id+"' src='"+offImg+"' onClick='SwitchLight(\""+id+"\");' style='cursor:pointer ' alt='"+describe+"' onMouseOver='WS(\""+describe+"\");' onMouseLeave='WS();'></div>";
  return o;
  //DW(o);
}

function SwitchLight(id)
{
  var e = GE("LIGHT_PIC_"+id);
  //alert(e);
  if (e != null)
  {
    var obj = LightAry["LIGHT_"+id];
    if (obj != null)
    {
      //alert(obj);
      e.src = obj.switchNext();
    }
  }
}
function GetLight(id)
{
  //return 0;
  //disable buttons
  var e = LightAry["LIGHT_"+id];
  var res = 0;
  if (e != null)
  {
    res = e.value;
  }
  //alert(e.lowsrc);
  return res;
}
function SetLight(id,val)
{
  //if(id == "gpInputIcon2" && val!=0) { alert(GetLight(id)); }
  if (GetLight(id) != val)
  {
    //alert(GetLight(id));
    SwitchLight(id);
  }
}
function SetLightPos(id,x,y)
{
  var e = GE("LIGHT_"+id);
  if (e != null)
  {
    e.style.left=x;
    e.style.top=y;
  }
  //alert(e+":"+x+":"+y);
}
// the light information object
function LightInfo(id,onImg,offImg,chgFun)
{
  this.id = id;
  this.onImg = onImg;
  this.offImg = offImg;
  this.value = 0;
  this.onChangeFunc = chgFun;
  this.switchNext = LightSwitchNext;
}
// switch to next status, and return the new image.
function LightSwitchNext()
{
  var res;
  if (this.value == 0)
  {
    this.value = 1;
    res = this.onImg;
  }
  else
  {
    this.value = 0;
    res = this.offImg;
  }
  if (this.onChangeFunc != null)
  {
    this.onChangeFunc();
  }
  return res;
}
var LightAry = {};

//===========================================
// about Machine type
//===========================================
function IsVS()
{
  var res = false;
  //res = (g_machineCode == "1290" ||g_machineCode == "1280");
  res = (g_machineCode == "1290" || g_machineCode == "1291");
  
  return res;
}

//===========================================
// Dynamic get content. XMLHttpRequest...
//===========================================
// use XMLHttpRequest to get the content, and put in center web page.
// isForceChange means do not check last page name, just reload it. 
function ChangeContent(link,isNoHis,isForceChange)
{
  if (link != null)
    link = link.toLowerCase();
  //WS((debugC++)+" : "+link);
  if (IsInBadLinkList(link))
    return;
  if (g_lockLink)
  {
    WS("LOCKED...");
    return;
  }
  g_lockLink = true;
  if (link == null)
  {
    link = CONTENT_PAGE;
  }
  else
  {
    CONTENT_PAGE_LAST = CONTENT_PAGE;
    if (isForceChange != true && CONTENT_PAGE==link)
    {
      //alert("link:"+link+":CP:"+CONTENT_PAGE);
      //avoid endless loop
      g_lockLink = false;
      return;
    }

    if (isNoHis != true)
    {
      if (g_backList.length > 0)
      {
        if (g_backList[g_backList.length-1] != link)
        {
          if (g_fwdList.length > 0 )
          {
            //alert(g_fwdList[g_fwdList.length-1]+"(11pop):"+g_backList[g_backList.length-1]+"(push)");
            g_backList.push(g_fwdList.pop());
          }
          //alert("11:g_backList(push) :"+link);
          g_backList.push(link);
        }
        
      }
      else
      {
        if (g_fwdList.length > 0 )
        {
          //alert(g_fwdList[g_fwdList.length-1]+"(22pop):"+g_backList[g_backList.length-1]+"(push)");
          g_backList.push(g_fwdList.pop());
        }

        //alert("22:g_backList(push) :"+link);
        g_backList.push(link);
      }
//      if (g_fwdList.length > 0 )
//      {
//        g_backList.push(g_fwdList.pop());
//      }
      g_fwdList = new Array();
    }

  }
  //20061211 move StopActiveX() to last, if you repeat to click the "image"
  // it will let ActiveX to stop , and can not restart.
  try
  {
    StopActiveX();
  }catch(e){};
  
  CONTENT_PAGE = link;
  if (WCH != null)
  {
    //WCH.onreadystatechange=null;
    delete WCH;
    WCH = null;
  }
  WCH = InitXHttp();
  WCH.onreadystatechange = OnWebContentProcess;
  try
  {
    WCH.open("GET", "/"+link, true);
    WCH.setRequestHeader("If-Modified-Since","0");
  //finally send the call
    WCH.send(null);
//alert("ChangeContent:"+data);
  }
  catch(e)
  {
    alert(GL("err_get_content"));
    CONTENT_PAGE = CONTENT_PAGE_LAST;
  }

  //g_lockLink = false;
}

function FoundBadLink(link)
{
  if (link == null || link == "") return true;
  var badStr=":/.%$#@!~^&*(){}|\\;\"'?><,";
  for (var i=0;i<badStr.length;i++)
  {
    if (link.indexOf(badStr.charAt(i)) >=0)
      return true;
  }
  return false;
}

//ALC , After Load Content
//Note: this function must call in the end of the each page. 
function ALC()
{
  WS("");
  g_lockLink = true;
  setTimeout(MY_ONLOAD, 300);
}

function GetViewCHHtml()
{
  var o='';
  if (IsVS())
  {
    o+='<div id="viewChannelLayer" class="cssViewChLayer">';
    o+=GL('view')+': '+GetCHSelHtml();
    o+='</div>';
  }
  return o;
}

function OnWebContentProcess()
{
  if (WCH == null)
    return;
  if (WCH.readyState == 4 )
  {
    if (WCH.status == 200 || WCH.status==401 || WCH.status==404 || WCH.status==403)
    {
      //alert(WCH.responseText);
      if (WCH.responseText.indexOf("var.js") >=0 )
        return;
      //var body = GE("WebContent");
      //var o = '<span style="display: none">'+I18NHtml(WCH.responseText)+'</span>';
      var o = '';
      //o+=GetViewCHHtml()+GetEmptyCallback()+I18NHtml(WCH.responseText);
      o+=GetEmptyCallback()+I18NHtml(WCH.responseText);
      //alert(o);
      
      // call unload function.
      CallOnUnload();
      //set_innerHTML('WebContent',o);
      setInnerHTML(GE('WebContent'),o);
      setTimeout(CallOnResize,500);
      //GE("WebContent").innerHTML='<span style="display: none">'+I18NHtml(WCH.responseText)+'</span>';
      if (WCH.status != 200)
      {
        g_lockLink = false;
      }
    }
    else
    {
      alert(GL("err_get_content"));
      CONTENT_PAGE = CONTENT_PAGE_LAST;
      g_lockLink = false;
    }
  }
}

function GetEmptyCallback()
{
  var o='';
  o+='<scri'+'pt>function MY_ONUNLOAD(){};function MY_ONLOAD(){g_lockLink = false;};function MY_CH_CHANGE(){};';
  o+='function MY_SUBMIT_OK(){};function MY_ONRESIZE(){};function MY_BEFORE_SUBMIT(){};</scri'+'pt>';
  return o;
}

//===========================================
// call to content script.
//===========================================
function CallOnUnload()
{
  //alert("CallOnUnload");
  g_dmsRun = false;
  clearTimeout(jpegTimer);
  jpegTimer = null;


  if (typeof(MY_ONUNLOAD) == 'function')
    MY_ONUNLOAD();
}

function CallOnResize()
{
  //alert("CallOnResize");
  var halfWidth = document.body.offsetWidth / 2;
  var obj = null;
  obj = GE("logoLayer");
  if (obj != null)
  {
    var tmpX = halfWidth - 370;
    if (g_brandName == "ipx")
    {
      tmpX += 30;
    };
    obj.style.left = tmpX;
  }
  obj = GE("arrowImg");
  if (obj != null)
  {
    obj.style.left = halfWidth + 270;
    obj.style.top = 520;
    var l,r;
    if (CONTENT_PAGE=="main.htm" || CONTENT_PAGE=="c_help.htm")
    {
      g_backList = new Array();
      g_fwdList = new Array();
      obj.style.left = -600;
    }
    else
    {
      var b = (g_backList.length > 0);
      if (g_backList.length == 1 && g_backList[0] == CONTENT_PAGE) b=false;
      var f = (g_fwdList.length > 0);
      if (b && f)
      {
        l=0;
        r=90;
      }
      else if (b)
      {
        l=0;
        r=60;
      }
      else if (f)
      {
        l=30;
        r=90;
      }
      else
      {
        l=30;
        r=60;
      }
      obj.style.clip="rect(0 "+r+" 30 "+l+")";
    }

  }
  if (IsVS())
  {
    obj = null;
    obj = GE("viewChannelLayer");
    if (obj != null)
    {
      obj.style.left = halfWidth + 250;
    }
    SetVisible("viewChannelLayer",(g_CHPageList.indexOf(CONTENT_PAGE+";") >= 0));
  }
  if (g_isShowPtzCtrl)
  {
    FixPtzButtonPos((CONTENT_PAGE=="main.htm"));
  }
  if (typeof(MY_ONRESIZE) == 'function')
    MY_ONRESIZE();
  
  //g_lockLink=false;
}

//===========================================
// Wrap the control to object.
//===========================================
//SetCCV : SetCommonContrlValue
function SetCCV(id,val)
{
  //alert(id);
  SetCtrlValue(CTRLARY,id,val);
}
function SetCtrlValue(ctrlList,id,val)
{
  var obj = ctrlList[id];
  if (obj != null)
  {
    obj.SV(val);
  }
}
//GetCCV : GetCommonContrlValue
function GetCCV(id)
{
  return GetCtrlValue(CTRLARY,id);
}
function GetCtrlValue(ctrlList,id)
{
  var res = '';
  var obj = ctrlList[id];
  if (obj != null)
  {
    res = encodeURIComponent(obj.GV());
  }
  return res;
}
//Set Control InActive 
function SetCIA(id,val)
{
  CTRLARY[id].active = (val==true);
/*
  var obj = CTRLARY[id];
  if (obj != null)
  {
    obj.active = (val==true);
  }
  else
  {
    alert("ERROR: SetCIA():"+id);
  }
*/  
}
function GetCtrlHtml(ctrlList, id)
{
  var res = '';
  var obj = ctrlList[id];
  if (obj != null)
  {
    if (obj.type == 'radio')
    {
      res = GetRadioONOFF(ctrlList,id);
    }
    else
    {
      res = obj.html;
    }
  }
  return res;
}

function WH(id)
{
  DW(WH_(id));
}
function WH_(id)
{
  return GetCtrlHtml(CTRLARY, id);
}
function GetRadioCtrlHtml(ctrlList, id, val)
{
  var res = '';
  var obj = ctrlList[id];
  if (obj != null)
  {
    res = obj.GetHtml(val);
  }
  return res;
}

function WRH(id,val)
{
  DW(WRH_(id,val));
}
function WRH_(id,val)
{
  return GetRadioCtrlHtml(CTRLARY, id,val);
}
function CPASS(ctrl,isQuite)
{
  var res = true;
  if (ctrl.checker != null)
  {
    res = ctrl.checker.IsPass(ctrl,isQuite);
  }
  return res;
}
function Ctrl_Select(id,list,val,setcmd,onChangeFunc,checker,inactive)
{
  this.type = "select";
  this.active = !(inactive==true);
  this.id = id;
  this.list = list;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.html = SelectObjectNoWrite(id,list,val,onChangeFunc);
  //GetValue
  this.GV = function (){ return GetValue(this.id); };
  //SetValue
  this.SV = function (val){ SetValue(this.id,val); };
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};

}
function Ctrl_SelectNum(id,sNum,eNum,gap,val,setcmd,onChangeFunc,onFocusFunc,fixNumC,checker,inactive)
{
  this.type = "selectNum";
  this.active = !(inactive==true);
  this.id = id;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.html = GetSelectNumberHtml(id,sNum,eNum,gap,val,onChangeFunc,onFocusFunc,fixNumC);
  //GetValue
  this.GV = function (){ return GetValue(this.id); };
  //SetValue
  this.SV = function (val){ SetValue(this.id,val); };
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};

}
function Ctrl_Text(id,size,maxlen,val,setcmd,checker,isPwd,onChangeFunc,inactive,onKeyUP)
{
  this.type = "text";
  this.active = !(inactive==true);
  this.id = id;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.html = CreateTextHtml(id,size,maxlen,val,isPwd,onChangeFunc,onKeyUP);
  //GetValue
  this.GV = function (){ return GetValue(this.id); };
  //SetValue
  this.SV = function (val){ SetValue(this.id,val); };
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};

}
function Ctrl_Check(id,val,setcmd,onClickFunc,checker,inactive)
{
  this.type = "check";
  this.active = !(inactive==true);
  this.id = id;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.onClickFunc = onClickFunc;
  this.GetHtml = function ()
  {
    var o='';
    o+='<input type="checkbox" id="'+this.id+'" name="'+this.id+'" ';
    if ( this.value == 1)
    {
      o+= "checked ";
    }
    if ( this.onClickFunc != null)
    {
      o+= "onClick='"+this.onClickFunc+"'";
    }
    o+= " >";
    return o;
  };
  this.html = this.GetHtml();
  //GetValue
  this.GV = function (){ return Bool2Int( IsChecked(this.id) ); };
  //SetValue
  this.SV = function (val){ SetChecked(this.id,(val==1)); };
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};

}
function Ctrl_Radio(id,val,setcmd,onClickFunc,checker,inactive)
{
  this.type = "radio";
  this.active = !(inactive==true);
  this.id = id;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.onClickFunc = onClickFunc;
  this.GetHtml = function (val)
  {
    var o = "<input type='radio' name='"+this.id+"' id='"+this.id+"' class='m1' value='"+val+"' ";
    if ( this.value == val)
    {
      o+= "checked ";
    }
    if ( this.onClickFunc != null)
    {
      o+= "onClick='"+this.onClickFunc+"'";
    }
    o+= " >";
    return o;
  };
  //GetValue
  this.GV = function (){ return GetRadioValue(this.id); };
  //SetValue
  this.SV = function (val){ SetRadioValue(this.id,val); };
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};

}
function GetRadioONOFF(id)
{
  return GetRadioONOFF(CTRLARY,id);
}
function GetRadioONOFF(list,id)
{
  var o='';
  o+=GetRadioCtrlHtml(list,id,1);
  o+=GL("on");
  o+=GetRadioCtrlHtml(list,id,0);
  o+=GL("off");
  return o;
}
//enable or disable
function GetRadioENDIS(id)
{
  var o='';
  o+=WRH_(id,1);
  o+=GL("enable");
  o+=WRH_(id,0);
  o+=GL("disable");
  return o;
}
//AD: Allow/Deny
function GetRadioAD(id)
{
  var o='';
  o+=WRH_(id,1);
  o+=GL("allow");
  o+=WRH_(id,0);
  o+=GL("deny");
  return o;
}
//note: onChangeFunc not implement
function Ctrl_IPAddr(id,val,setcmd,onChangeFunc,checker,inactive)
{
  this.type = "ipaddr";
  this.active = !(inactive==true);
  this.id = id;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.onChangeFunc = onChangeFunc;
  this.GetIP = function (s){
    if (s.charAt(0)=='0')
    {
      s = s.replace('0','');
      if (s.charAt(0)=='0')
      {
        s = s.replace('0','');
      }
    }
    if (s == "") return 0;
    return parseInt(s);
  };
  this.GV = function (){
    var o = '';
    for (var i=1;i<5;i++)
    {
      try
      {
        o+=FixNum( this.GetIP(GE(this.id+'_ip'+i).value) , 3);
        o+=(i<4)?".":"";
      }catch (e){};
    }
    this.value = o;
	
    //alert(o.replace(/\./g,""));
    return o.replace(/\./g,"");
  };
  this.SV = function (val){
    var ips = val.split(".");
    if(ips.length>=4)
    {
      for (var i=1;i<5;i++)
      {
        try
        {
          GE(this.id+'_ip'+i).value = this.GetIP(ips[i-1]);
        }catch (e){};
      }
      this.value = val;
    }
  };
  this.GetHtml = function (){
    var ips = this.value.split(".");
    //alert(ips.length);
    var o='';
    o+='<div class="cssIpAddr" id="'+this.id+'">';
    for (var i=1;i<5;i++)
    {
      //alert(ips[i-1]);
      o+='<input type="text" value="'+((ips.length>=4)?this.GetIP(ips[i-1]):'0')+'" name="'+this.id+'_ip'+i+'" id="'+this.id+'_ip'+i+'" maxlength=3 class="cssIpAddrItem" onkeyup="IPMask(this,event)" onkeydown="IPMaskDown(this,event)" onblur="if(this.value==\'\')this.value=\'0\';" onbeforepaste=IPMask_c()>';
      o+=(i<4)?".":"";
    }
    o+='</div>';
    return o;
  };
  this.html = this.GetHtml();
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};
  this.Disable = function (isDis)
  {
    this.active = !isDis;
    for (var i=1;i<5;i++)
      DisableObject(this.id+'_ip'+i,isDis);
  }

}
//note: onChangeFunc not implement
function Ctrl_IPFilter(id,val,setcmd,onChangeFunc,checker,inactive)
{
  this.type = "ipfilter";
  this.active = !(inactive==true);
  this.id = id;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.onChangeFunc = onChangeFunc;
  this.GetIP = function (s){
    if (s.charAt(0)=='0')
    {
      s = s.replace('0','');
      if (s.charAt(0)=='0')
      {
        s = s.replace('0','');
      }
    }
    if (s == "") return 0;
    //return parseInt(s);
    return s;
  };
  this.GV = function (){
    var o = '';
    for (var i=1;i<5;i++)
    {
      try
      {
        o+=this.GetIP(GE(this.id+'_ip'+i).value);
        o+=(i<4)?".":"";
      }catch (e){};
    }
    this.value = o;
    return o;
  };
  this.SV = function (val){
    var ips = val.split(".");
    if(ips.length>=4)
    {
      for (var i=1;i<5;i++)
      {
        try
        {
          GE(this.id+'_ip'+i).value = this.GetIP(ips[i-1]);
        }catch (e){};
      }
      this.value = val;
    }
  };
  this.GetHtml = function (){
    var ips = this.value.split(".");
    //alert(ips.length);
    var o='';
    o+='<div class="cssIpFilter" id="'+this.id+'">';
    for (var i=1;i<5;i++)
    {
      //alert(ips[i-1]);
      o+='<input type="text" value="'+((ips.length>=4)?this.GetIP(ips[i-1]):'0')+'" name="'+this.id+'_ip'+i+'" id="'+this.id+'_ip'+i+'" maxlength=9 class="cssIpFilterItem" onkeyup="IPMask(this,event,true)" onkeydown="IPMaskDown(this,event)" onblur="if(this.value==\'\')this.value=\'0\';" onbeforepaste=IPMask_c(true)>';
      o+=(i<4)?".":"";
    }
    o+='</div>';
    return o;
  };
  this.html = this.GetHtml();
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};
  this.Disable = function (isDis){
    for (var i=1;i<5;i++)
      DisableObject(this.id+'_ip'+i,isDis);
  };

}
function Ctrl_IPFilter_Dlink(id,val,setcmd,onChangeFunc,checker,inactive)
{
  this.type = "ipfilter";
  this.active = !(inactive==true);
  this.id = id;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.onChangeFunc = onChangeFunc;
  this.GetIP = function (s){
    if (s.charAt(0)=='0')
    {
      s = s.replace('0','');
      if (s.charAt(0)=='0')
      {
        s = s.replace('0','');
      }
    }
    if (s == "") return 0;
    //return parseInt(s);
    return s;
  };
  this.GV = function (){
    var o = '';
    for (var i=1;i<5;i++)
    {
      try
      {
        o+=this.GetIP(GE(this.id+'_ip'+i).value);
        o+=(i<4)?".":"";
      }catch (e){};
    }
    this.value = o;
    return o;
  };
  this.SV = function (val){
    var ips = val.split(".");
    if(ips.length>=4)
    {
      for (var i=1;i<5;i++)
      {
        try
        {
          GE(this.id+'_ip'+i).value = this.GetIP(ips[i-1]);
        }catch (e){};
      }
      this.value = val;
    }
  };
  this.GetHtml = function (){
    var ips = this.value.split(".");
    //alert(ips.length);
    var o='';
    o+='<div class="cssIpFilter_dlink" id="'+this.id+'">';
    for (var i=1;i<5;i++)
    {
      //alert(ips[i-1]);
      o+='<input type="text" value="'+((ips.length>=4)?this.GetIP(ips[i-1]):'0')+'" name="'+this.id+'_ip'+i+'" id="'+this.id+'_ip'+i+'" maxlength=5 class="cssIpFilterItem_dlink" onkeyup="IPMask(this,event,true)" onkeydown="IPMaskDown(this,event)" onblur="if(this.value==\'\')this.value=\'0\';" onbeforepaste=IPMask_c(true)>';
      o+=(i<4)?".":"";
    }
    o+='</div>';
    return o;
  };
  this.html = this.GetHtml();
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};
  this.Disable = function (isDis){
    for (var i=1;i<5;i++)
      DisableObject(this.id+'_ip'+i,isDis);
  };

}
//===========================================
// IP Mask control
//===========================================
function FocusToNextIPAddr(obj,isNext,select)
{
  //alert(obj+":"+isNext);
  obj.blur();
  var nextip=parseInt(obj.name.charAt(obj.name.length-1));
  nextip=(isNext)?nextip+1:nextip-1;
  nextip=nextip>=5?1:nextip;
  nextip=nextip<=0?4:nextip;
  try
  {
    var oo = GE(obj.name.substring(0,obj.name.length-1)+nextip);
    oo.focus();
    if (select)
    {
      oo.select();
    }
  }catch (e){};

}
function IPMaskDown(obj,evt)
{
  var key1 = evt.keyCode;
  if(key1 == 190 || key1 == 110)
  {
    FocusToNextIPAddr(obj,true,false);
  }
}
function IPMask(obj,evt,isFilter)
{
  var key1 = evt.keyCode;
  //alert(key1);
  if (!(key1==46 || key1==8 || key1==48 || (key1>=49 && key1<=57) || (key1>=97 && key1<=104) ))
  {
    if (isFilter == true)
    {
      obj.value=obj.value.replace(/[^\d\-\*\(\)]/g,'');
    }
    else
    {
      obj.value=obj.value.replace(/[^\d]/g,'');
    }
  }

  if(key1 == 190 || key1 == 110)
  {
    obj.select();
  }
  //alert(key1);
  if( key1==37 || key1==39 )
  {
    //alert("a:"+key1);
    FocusToNextIPAddr(obj,(key1==39),false);
  }  
  if(obj.value.length>=3)
  {
    if(parseInt(obj.value)>=256  ||  parseInt(obj.value)<0)
    {
      alert(GL("err_ip_address"));
      obj.value="";
      obj.focus();
      return  false;
    }
//    else  
//    {
      //alert("b:"+key1+":"+obj.value);
      //FocusToNextIPAddr(obj,true,true);
//    }
  }
}
function IPMask_c(isFilter)
{
  var txt = clipboardData.getData('text');
  txt = (isFilter==true)?txt.replace(/[^\d\-\*\(\)]/g,''):txt.replace(/[^\d]/g,'');
  clipboardData.setData('text',txt);
}
//===========================================
// validate control and submit
//===========================================
function GetSetterCmd(ctrl,val)
{
  return GetSetterCmdKV(ctrl.setcmd,val);
}
//direct input the name and value
function GetSetterCmdKV(name,val)
{
  var o = "&"+name;
  //Luther memo: it is important, null string is equals 0, :(
  if (isNaN(parseInt(val)) && val=="")
  {
    val = "(null)";
  }
  //if setcmd contain "=" ,it's mean video server do not add channel id.
  if (name.indexOf("=")<0)
  {
    o+="=";
    if(IsVS())
    {
      o += g_CHID+":";
    }
  }
  //add value
  o += val;
  return o;
}
function ValidateCtrlAndSubmit(ctrlList,isAsync)
{
  if (g_lockLink)
  {
    WS("LOCKED...");
    return;
  }

  if (isAsync == null) isAsync=false;
  //validate
  var res = true;
  for(var key in ctrlList)
  {
    var obj = ctrlList[key];
    if (obj.active)
    {
//      if (obj.checker != null)
//      {
//        res = obj.checker.IsPass(key,CTRLARY);
        res = obj.IsPass();
        if (!res)
        {
          return res;
        }
//      }
    }
  }

  //call back
  if ( MY_BEFORE_SUBMIT() == false )
  {
    return false;
  }

  //submit ,
  g_httpOK = true;
  var o = c_iniUrl;
 // var count=0;
  var len = g_MaxSubmitLen;
  for(var key in ctrlList)
  {
    var obj = ctrlList[key];
    if (obj.active)
    {
      var str = GetSetterCmd(obj,GetCtrlValue(ctrlList,key));
//alert(str);
      len -= str.length;
      o+=str;
//alert(g_MaxSubmitLen+"::::"+len);
      if (len < 10 )
      {
        //send submit
//alert(o);
        SendHttp(o,isAsync);
        if (g_httpOK)
        {
          o = c_iniUrl;
          len = g_MaxSubmitLen;
        }
        else
        {
          break;
        }
      }
  //    count+=1;
    }
  }

  if (len != g_MaxSubmitLen)
  {
//alert(o);
    //SendHttp(encodeURIComponent(o),isAsync);
    SendHttp(o,isAsync);
  }

  if (g_httpOK)
  {
    MY_SUBMIT_OK();
  }


}
//===========================================
// Dispaly Message on browser status bar
//===========================================
function WS(msg)
{
  window.status = (msg==null)?"":msg;
}
//===========================================
// about video server channel
//===========================================
// Get the channel id from the global variable "g_CHID".
function GetCHSelHtml()
{
  var o="";
  o+="<select onChange='ChangeViewCH(this.value)' id='viewCHSel' class='m1'>";
  for (var i=0;i<5;i++)
  {
    o+="<option value='"+i+"' "+((i==g_CHID)?"selected":"")+">"+((i==0)?"---":i)+"</option>";
  }
  o+="</select>";
  return o;
}
function ChangeViewCH(id)
{
  //focus lost
  window.focus();
  GE("viewCHSel").blur();
  
  if (g_CHID == 0 || id == 0)
  {
    if (CONTENT_PAGE == "main.htm")
    {
      g_CHID=id;
      ChangeContent();
    }
    else
    {
      // no change g_CHID
      SetValue("viewCHSel",g_CHID);
      MY_CH_CHANGE();
      //20070419 Luther add 1 line
      ChangeChannelTextPanel(g_CHID);
    }
  }
  else
  {
    g_CHID=id;
    MY_CH_CHANGE();
    //20070419 Luther add 1 line
    ChangeChannelTextPanel(g_CHID);
  }
}
function GetLogoHtml()
{
  var o='';
  if (g_brandName != "nobrand")
  {
    o+='<div id="logoLayer" class="cssLogoLayer" >';
    var hasUrl = !(g_brandUrl == null || g_brandUrl == "" || g_brandUrl == "(null)");
    if (hasUrl)
    {
      o+='<a href="'+g_brandUrl+'" target="_blank">';
    }
    o+='<img border=0 src="'+g_brandName+'.gif">';
    if (hasUrl)
    {
      o+='</a>';
    }
    o+='</div>';
  }
  return o;
}
//===========================================
// Common comtent
//===========================================
//PH : Page Header
function GetPHHtml(key,bodytxt)
{
  return GetPHTxtHtml(GL(key),bodytxt);
}
//show text, not keyword.
function GetPHTxtHtml(txt,bodytxt)
{
  // 20090205 Netpool Modified for D-Link Style
  var o='';
  o+='<table class="maincontent" align="center" border="0" cellspacing="0" cellpadding="0" >';
  o+='<tr><td class="box_header" >'+txt+'</td></tr>';
  o+='<tr><center><td class="box_body" >';
  if(bodytxt == null)
    o+='&nbsp;';
  else
    o+=bodytxt;
  o+='</td></center></tr>';
  o+='</table>';
  return o;
}
function WritePH(key,bodytxt)
{
  DW(GetPHHtml(key,bodytxt));
}
function WriteTxtPH(txt)
{
  DW(GetPHTxtHtml(txt));
}
// 20090206 Netpool Add for D-Link Style
function WriteImagePH(useAX,isOK)
{
  DW('<table class="maincontent" align="center" border="0" cellspacing="0" cellpadding="0" >' );
  DW('<tr><td class="page_header">Live Video</td></tr>');
  DW('<tr><td class="page">');

  DW('<table border=0 cellspacing=0 cellpadding=0 align="center" >');
  DW('<tr height="20" bgcolor="#000000"><td align="left" ><span id="devTitleLayer" class="text1" >'+g_titleName+'</span></td></tr>');
  DW('<tr><td><center><div id="imgAX">');
  if (isOK)
    DW(GetImageLeftVideo(useAX));
  else
    DW("<img id='sampleimg' name='sampleimg' src='/Loading.gif' width=352 height=240 border=2>");
  DW('</div></center></td></tr></table>');

  DW('</td></tr></table>');
};
//===========================================
// Common Image Content
//===========================================
//useAX: use ActiveX control ? yes or not, 
//isOK: if in changing mode. isOK is false.
//PH : Page Header
function WriteImageTxtPH(txtTitle,useAX,isOK)
{
  DW('<table border="0" cellspacing="1" cellpadding="0" width="880" height="500" align="center" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >');
  //20090203 Netpool Modified for D-Link Style
  DW('<tr><td id="sidenav_container" align="left" vAlign="top" >');
  //20090228 chirun del
  //DW(CommonGetMenuStr(MENU_ITEM_IMAGE));
  DW(GetDLinkMenuStr(MENU_ITEM_STATUS, addMenu));
  DW('</td><td align="left" vAlign="top" width="650" >');

  var headTxt='Here is The Text of Head.';
  var bodyHTML='Here is HTML of body what you want to write.';
  DW(GetDLinkBox(headTxt,"000000","ff6f00",bodyHTML,"000000","dfdfdf",600));
  headTxt='Here is The 2th Text of Head.';
  bodyHTML='Here is the 2th HTML of body what you want to write.';
  DW(GetDLinkOrangeBox(headTxt,bodyHTML));
  headTxt='Here is The 3th Text of Head.';
  bodyHTML='Here is the 3th HTML of body what you want to write.';
  DW(GetDLinkBlackBox(headTxt,bodyHTML));

  //WriteTxtPH(txtTitle);
  //WriteImagePH(useAX,isOK);
  WriteDLinkTxtTablePH(txtTitle);
}
//PB : Page Bottom
function WriteImagePB()
{
  //20090206 Netpool Modified for D-Link Style
  WriteDLinkTablePB();
  DW('</td>');
  DW('<td id="sidenav_container" align="left" vAlign="top" >');
  DW('</td></tr></table>');
}

function VS_NO_VIEW_ALL()
{
  if (g_CHID==0) g_CHID = 1;
  if (IsVS())
  {
    try
    {
    GE('viewCHSel').selectedIndex = g_CHID;
    }catch(e){};
  }
}
function ImageOnLoad(isNoShowWaitImg,useAX,codec,mode,forceWait)
{
  VS_NO_VIEW_ALL();
  //CallOnResize();
//alert(mode);
  if (isNoShowWaitImg)
  {
    if (useAX==1 || IsMpeg4())
    {
      if (!IsVS())
      {
        StartActiveXEx(0,0,codec,null,null,mode,forceWait);
        //video server will start in MY_CH_CHANGE
      }
    }
    else
    {
      imgFetcher.RunDms();
    }
  }
  if (IsVS())
  {
    MY_CH_CHANGE();
  }
};
//===========================================
// Common Netowrk Content
//===========================================
//PH : Page Header
function WriteNetPH(titleKey,extMenu)
{
  //20090206 Netpool Modified for D-Link Style
  DW('<table border="0" cellspacing="1" cellpadding="0" width="900" height="500" align="center" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >');
  DW('<tr><td id="sidenav_container" align="left" vAlign="top" >');
  DW(CommonGetMenuStr(MENU_ITEM_NETWORK, extMenu));
  DW('</td><td align="left" vAlign="top" width="650" >');
  WritePH(titleKey);
  WriteDLinkTablePH(titleKey);
}
//PB : Page Bottom
function WriteNetPB()
{
  //20090206 Netpool Modified for D-Link Style
  WriteDLinkTablePB();
  DW('</td>');
  DW('<td id="sidenav_container" align="left" vAlign="top" >');
  DW('</td></tr></table>');
}
function WIPX(tid,ctx)
{
  DW('<tr class="b1"><td width=150 height="30" >');
  DW(GL(tid)+':</td><td >'+ctx+'</td></tr>');
}
function WIP(tid,id)
{
  WIPX(tid,WH_(id));
}
function WIP1(ctx,css)
{
  DW('<tr class="'+((css==null)?"b1":css)+'" width=150><td colspan=2 height="30" >');
  DW( ctx+'</td></tr>');
}                             
function WIPSubmit(isAsync)
{
  DW("<tr><td colspan=2 align=center><br>");
  CreateSubmitButton(null,isAsync);
  DW("</td></tr>");
}
//===========================================
// Common Application Content
//===========================================
//PH : Page Header
function WriteAppPH(titleKey)
{
  return WriteAppTxtPH(GL(titleKey))
}
function WriteAppTxtPH(txtTitle)
{
  //20090206 Netpool Modified for D-Link Style
  DW('<table border="0" cellspacing="1" cellpadding="0" width="900" height="500" align="center" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >');
  DW('<tr><td id="sidenav_container" align="left" vAlign="top" >');
  DW(GetMenuAppStr());
  DW('</td><td align="left" vAlign="top" width="650" >');
  WriteTxtPH(txtTitle);
  WriteDLinkTxtTablePH(txtTitle);
}

//PB : Page Bottom
function WriteAppPB()
{
  //20090206 Netpool Modified for D-Link Style
  WriteDLinkTablePB();
  DW('</td>');
  DW('<td id="sidenav_container" align="left" vAlign="top" >');
  DW('</td></tr></table>');
}
function WTablePH(w)
{
  DW("<center><table width="+((w!=null)?w:"450")+" border=0 cellPadding=0 cellSpacing=0>");
}
function WTablePB()
{
  DW("</table></center>");
}
function WIApp(id,enid,ctx)
{
  DW("<tr><td height=30 class='b1'>");
  DW(WH_(id)+" "+GL(enid)+" - "+ctx+"");
  DW("</td></tr>");
}
function WIApp1(ctx)
{
  DW("<tr><td height=30 class='b1' style='text-indent:3em'>"+ctx+"</td></tr>");
}
function WIAppSubmit()
{
  DW("<tr><td ><center><br>")
  CreateSubmitButton();
  DW("</td></tr>");
}
//===========================================
// Common System Content
//===========================================
//PH : Page Header
function WriteSysPH(titleKey,addMenu)
{
  return WriteSysTxtPH(GL(titleKey),addMenu);
}
function WriteSysTxtPH(txtTitle,addMenu)
{
  //20090206 Netpool Modified for D-Link Style
  DW('<table border="0" cellspacing="1" cellpadding="0" width="900" height="500" align="center" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >');
  DW('<tr><td id="sidenav_container" align="left" vAlign="top" >');
  DW(CommonGetMenuStr(MENU_ITEM_SYSTEM, addMenu));
  DW('</td><td align="left" vAlign="top" width="650" >');
  WriteTxtPH(txtTitle);
  WriteDLinkTxtTablePH(txtTitle);
}
//PB : Page Bottom
function WriteSysPB()
{
  //20090206 Netpool Modified for D-Link Style
  WriteDLinkTablePB();
  DW('</td>');
  DW('<td id="sidenav_container" align="left" vAlign="top" >');
  DW('</td></tr></table>');
}
//===========================================
// Common Status chirun add for D-link(7315)
//===========================================
//PH : Page Header
function WriteStatusPH(titleKey,addMenu)
{
  return WriteStatusTxtPH(GL(titleKey),addMenu);
}
function WriteStatusTxtPH(txtTitle,addMenu)
{
  DW('<table border="0" cellspacing="1" cellpadding="0" width="900" height="500" align="center" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >');
  DW('<tr><td id="sidenav_container" align="left" vAlign="top" >');
  DW(CommonGetMenuStr(MENU_ITEM_STATUS, addMenu));
  DW('</td><td align="left" vAlign="top" width="650" >');
}
//PB : Page Bottom
function WriteStatusPB()
{
  //20090206 Netpool Modified for D-Link Style
  WriteDLinkTablePB();
  DW('</td>');
  DW('<td class="sidenav_container" align="left" vAlign="top" >');
  DW('</td></tr></table>');
}
//===========================================
// Common Maintenance chirun add for D-link(7315)
//===========================================
//PH : Page Header
function WriteMaintenancePH(titleKey,addMenu)
{
  return WriteMaintenanceTxtPH(GL(titleKey),addMenu);
}
function WriteMaintenanceTxtPH(txtTitle,addMenu)
{
  DW('<table border="0" cellspacing="1" cellpadding="0" width="900" height="500" align="center" bgcolor="#FFFFFF" bordercolordark="#FFFFFF" >');
  DW('<tr><td id="sidenav_container" align="left" vAlign="top" >');
  DW(CommonGetMenuStr(MENU_ITEM_MAINTENANCE, addMenu));
  DW('</td><td align="left" vAlign="top" width="650" >');
}
//PB : Page Bottom
function WriteMaintenancePB()
{
  WriteDLinkTablePB();
  DW('</td>');
  DW('<td id="sidenav_container" align="left" vAlign="top" >');
  DW('</td></tr></table>');
}
//===========================================
// Checker Object
//===========================================
//each checker has one validateObj and can link to another checker
function CheckerObj(validateObj, linkChecker)
{
  this.validateObj = validateObj;
  this.linkChecker = linkChecker;
  //this.ctrlList = ctrlList;
  this.IsPass = function(ctrlObj,isQuite)
  {
    var res = true;
    if (this.linkChecker != null)
    {
      res = this.linkChecker.IsPass(ctrlObj,isQuite);
    }
    if (res == true)
    {
      res = this.validateObj.IsPass(ctrlObj.GV(),isQuite);
    }
    if (res == false)
    {
      try
      {
        var obj = GE(ctrlObj.id);
        obj.focus();
        obj.select();
      }catch (e){};
    }
    return res;
  };
}
//ValidateObj must implement IsPass(isQuite) return ture or false
//check the string length ,range (minLen <= x <= maxLen)
function V_StrLen(minLen,maxLen,defMsg,msg)
{
  //this.ctrlID = ctrlID;
  this.defMsg = defMsg;
  this.msg = msg;
  this.minLen = minLen;
  this.maxLen = maxLen;
  this.IsPass = function(val,isQuite)
  {
    return !(CheckBadStrLen(val,this.minLen,this.maxLen,this.defMsg,this.msg,isQuite));
  };
}
//check the string is English or number
function V_StrEnglishAndNumber(defMsg,msg,noCheckNum,noCheckLower,noCheckUpper )
{
  //this.ctrlID = ctrlID;
  this.defMsg = defMsg;
  this.msg = msg;
  this.noNum = noCheckNum;
  this.noLower = noCheckLower;
  this.noUpper = noCheckUpper;
  this.IsPass = function(val,isQuite)
  {
    return !(CheckBadEnglishAndNumber(val,this.defMsg,this.msg,this.noNum,this.noLower,this.noUpper,isQuite));
  };
}
function V_NumRange(min,max,defMsg,msg,extra)
{
  this.min = min;
  this.max = max;
  this.defMsg = defMsg;
  this.msg = msg;
  this.extra = extra;
  this.IsPass = function(val,isQuite)
  {
    return !(CheckBadNumberRange(val,this.min,this.max,this.defMsg, this.msg, isQuite, this.extra));
  }
}
function V_Empty(defMsg,msg)
{
  this.defMsg = defMsg;
  this.msg = msg;
  this.IsPass = function(val,isQuite)
  {
    return !(CheckIsNull(val, this.defMsg, this.msg, isQuite));
  }
}
function V_EMail(msg)
{
  this.msg = msg;
  this.IsPass = function(val,isQuite)
  {
    return !(CheckBadEMail(val,this.msg,isQuite));
  }
}

function V_Hex()
{
  this.IsPass = function(val,isQuite)
  {
    return (CheckHex(val,isQuite));
  }
}

function V_Ip(msg)
{
  this.msg = msg;
  this.IsPass = function(val,isQuite)
  {
    return CheckIpFormat(val,this.msg,isQuite);
  }
}
//===========================================
// Global validate object
//===========================================
var gv_ipPort = new V_NumRange(1,65535,"");
var gco_ipPort = new CheckerObj(gv_ipPort);
var gv_byte = new V_NumRange(0,255,"");
var gco_byte = new CheckerObj(gv_byte);
var gv_empty = new V_Empty(GL("text_field"));
var gco_empty = new CheckerObj(gv_empty);
var gv_email = new V_EMail();
var gco_email = new CheckerObj(gv_email,gco_empty);
var gv_engNum = new V_StrEnglishAndNumber("");
var gco_engNum = new CheckerObj(gv_engNum,gco_empty);
var gv_hex = new V_Hex();
var gco_hex = new CheckerObj(gv_hex,gco_empty);
var gv_ip = new V_Ip("format_error");
var gco_ip = new CheckerObj(gv_ip);
function Hex2Dec(hex)
{
  return parseInt(hex,16);
}
function Dec2Hex(dec)
{
  return dec.toString(16).toUpperCase();
}
//===========================================
// CSS style position.
//===========================================
function SetPos(id,x,y,w,h)
{
  var obj = GE(id);
  if (obj != null && obj.style != null)
  {
    try
    {
      obj.style.width = w;
      obj.style.height = h;
      obj.style.top = y;
      obj.style.left = x;
    }catch(e){};
  }
}
//===========================================
// Index Page
//===========================================
//Luther add for CMS access
function IndexCMSContent()
{
  DW('<link href="lc_en_us.css" rel="stylesheet" type="text/css"></head><body><table><tr><td valign="top" width="745"><div id="WebContent" ></div></td></tr></table></body>');  if (THIS_PAGE != null && THIS_PAGE.indexOf("?")>=0)
  {
    var ss = THIS_PAGE.split("?");
    if (FoundBadLink(ss[1]))
    {
      ChangeContent(page);
    }
    else
    {
    //alert(ss[1]+":"+CONTENT_PAGE+":"+ss[0]);
    if (ss[0] == ss[1]+".htm")
      ChangeContent(page);
    else
      ChangeContent(ss[1]+".htm");
    }
  }

}

function IndexContent(page,noHead,index,extTitle)
{
  if (noHead != true)
  {
    WriteDLinkHtmlHead(extTitle,null,null,"CallOnUnload","CallOnResize",null,index);
  }
  WriteDLinkBottom(index);

  if (THIS_PAGE != null && THIS_PAGE.indexOf("?")>=0)
  {
    var ss = THIS_PAGE.split("?");
    if (FoundBadLink(ss[1]))
    {
      ChangeContent(page);
    }
    else
    {
    //alert(ss[1]+":"+CONTENT_PAGE+":"+ss[0]);
    if (ss[0] == ss[1]+".htm")
      ChangeContent(page);
    else
      ChangeContent(ss[1]+".htm");
    }
  }
  else
  {
    ChangeContent(page);
  }
}

function MoveOnArrow(evt)
{
  var obj = GE("arrowImg");
  if (obj != null)
  {
    var v = parseInt(evt.x) - parseInt(obj.style.left);
    if (v < 30)
    {
      WS(GL("back"));
    }
    else if (v < 60)
    {
      WS(GL("reload"));
    }
    else
    {
      WS(GL("forward"));
    }
  }
}
function ClickArrow(evt)
{
  var obj = GE("arrowImg");
  if (obj != null)
  {
    if (g_lockLink)
    {
      WS("LOCKED...");
      return;
    }
    var v = parseInt(evt.x) - parseInt(obj.style.left);
    if (v < 30)
    {
      if (g_backList.length > 0)
      {
        var k = g_backList.pop();
//alert("33 g_backList pop "+k);
        g_fwdList.push(k);
//alert("44 g_fwdList push "+k);
//alert("C:"+CONTENT_PAGE+" B:"+g_backList.length+" F:"+g_fwdList.length);
        while (k == CONTENT_PAGE)
        {
          k = g_backList.pop();
          g_fwdList.push(k);
//alert("55g_backList pop "+k);
        }
        if (k != null)
        {
//          g_backList.push(k);
          //g_fwdList.push(k);
//alert("66g_fwdList push "+k);
          ChangeContent(k,true);
        }
      }
    }
    else if (v < 60)
    {
      ChangeContent(null,true);
    }
    else
    {
      if (g_fwdList.length > 0)
      {
        var k = g_fwdList.pop();
//alert("77g_fwdList pop "+k);
        g_backList.push(k);
//alert("88g_backList push "+k);
//alert("C:"+CONTENT_PAGE+" B:"+g_backList.length+" F:"+g_fwdList.length);
        while (k == CONTENT_PAGE)
        {
          
          k = g_fwdList.pop();
          g_backList.push(k);
//alert("99g_fwdList pop "+k);
        }
        if (k != null)
        {
          //g_fwdList.push(k);
          //g_backList.push(k);
//alert("AAg_backList push "+k);
          ChangeContent(k,true);
        }
      }
    }

  }
  
}

//============About JPEG Viewer===========
//use AJAX to replace Java Applet.
var g_maxDmsObj = 16;
var g_fetchList = {};
var jpegCounter = 0;
var jpegTimer = null;
var g_dmsRun = false;
var zeroFpsCount = 0;

function ImageFetcher(myid)
{
  this.bufImg1 = new Image();
  this.bufImg2 = new Image();
  this.newImg = this.bufImg1;
  this.myID = myid;
  this.GetDmsImgStr = function (w,h)
  {
    //20070613 Luther fix 1280 * 960
    if (w > 720)
    {
      h = (720 * h) / w;
      w = 720;
    }
    return '<img src="" width="'+w+'" height="'+h+'" id="showdms_'+this.myID+'" border=0 >';
  };

  this.imgSwitch = function ()
  {
    if (this.newImg == this.bufImg1)
    {
      this.newImg = this.bufImg2;
    }
    else
    {
      this.newImg = this.bufImg1;
    }
  }
  
  this.RunDms = function ()
  {
    if (jpegTimer == null)
    {
      jpegTimer = setTimeout("JpegFrameCal()",1000);
    }
    g_dmsRun = true;
    //this.newImg = new Image();
    this.imgSwitch();
    //var d = g_dmsList[id];
    var obj = GE("showdms_"+this.myID);
    if (obj != null)
    {
	  
      this.newImg.id = "showdms_"+this.myID;
      this.newImg.onload=DmsOK;
      this.newImg.src="/dms?nowprofileid="+mpMode+"&"+Math.random();
    }
  };


}
function DmsOK()
{
  //id = 0;
  var z = 1;
  if (GE("zoomSel") != null)
  {
    z = parseInt(GetValue("zoomSel"));
  }
  //var obj = GE("showdms_"+this.myID);
  var obj = GE("showdms_0");
  if (obj != null)
  {
    var p = obj.parentNode;
    p.removeChild(obj);
    var b = imgFetcher.newImg;
    if (CONTENT_PAGE=="main.htm")
    {
      b.width=g_viewXSize*z;
      b.height = g_viewYSize*z;
    }
    else
    {
      b.width=obj.width;
      b.height = obj.height;
    }
    p.appendChild(b);
//alert("g_dmsRun="+g_dmsRun+" g_lockLink="+g_lockLink);
    if (g_dmsRun && !g_lockLink) setTimeout("imgFetcher.RunDms()",5);
    jpegCounter++;
  }

}


var imgFetcher = new ImageFetcher(0);
g_fetchList[0] = imgFetcher;
/*
function GetDmsImgStr(id,w,h)
{
  return '<img src="" width="'+w+'" height="'+h+'" id="showdms_'+id+'" border=0 >';
}
*/
/*
//id is from 0 to 15
function RunDms(id)
{
  if (jpegTimer == null)
  {
    jpegTimer = setTimeout("JpegFrameCal()",1000);
  }
  g_dmsRun = true;
  g_dmsList[id] = new Image();
  var d = g_dmsList[id];
  var obj = GE("showdms_"+id);
  if (obj != null)
  {

    d.id = "showdms_"+id;
    d.onload=DmsOK;
    d.src="/dms?"+Math.random();
  }
}
function DmsOK()
{
  //id = 0;
  var z = 1;
  if (GE("zoomSel") != null)
  {
    z = parseInt(GetValue("zoomSel"));
  }
  for (var i=0;i<g_maxDmsObj;i++)
  {
    var obj = GE("showdms_"+i);
    if (obj != null)
    {
      var p = obj.parentNode;
      p.removeChild(obj);
      var b = g_dmsList[i];
      if (CONTENT_PAGE=="main.htm")
      {
        b.width=g_viewXSize*z;
        b.height = g_viewYSize*z;
      }
      else
      {
        b.width=obj.width;
        b.height = obj.height;
      }
      p.appendChild(b);
      if (g_dmsRun && !g_lockLink) setTimeout("RunDms("+i+")",5);
      jpegCounter++;
    }
  }
}
*/
function JpegFrameCal()
{
  WS(jpegCounter+" fps");
  if (jpegCounter == 0)
  {
    zeroFpsCount ++;
  }
  else
  {
    zeroFpsCount = 0;
  }
  jpegCounter = 0;
  if (g_dmsRun && !g_lockLink)
  {
    jpegTimer = setTimeout("JpegFrameCal()",1000);
  }
  if (zeroFpsCount >= 10)
  {
    zeroFpsCount = 0;
    //this is not good.
//    alert("RESTART DMS");
    var n;
    for (n in g_fetchList)
    {
      var obj = GE("showdms_"+n);
      //alert("n="+n+" obj="+obj);
      if (obj != null)
      {
        g_fetchList[n].RunDms();
      }
    }
    
  }
}

function InitLoad()
{
  //alert(g_langName);
 // alert(g_mui);

  loadJS("/setInnerHTML.js");
  loadJS("/lang_"+g_langName+".jsl");
};

//you must prepare a <div> element,
//ex: <div id="pppoe_view" style="position:absolute; top:0;left:0;width:0;height:0; z-index:3; visibility:hidden"></div>
//
function PopView(divName,x,y,w,h,url)
{
  var obj = document.getElementById(divName);
  obj.style.top=y;
  obj.style.left=x;
  obj.innerHTML='<table ><tr><td style="border-width:thick;border-color:red"><iframe width='+w+' height='+h+' src="'+url+'" ></iframe></td></tr><tr><td align="center"><input name="PopClose" type="button" value="Close" onClick=PopViewClose("'+divName+'")></td></tr></table>';
}

function PopViewClose(divName)
{
  document.getElementById(divName).style.visibility='hidden';
}
function PopViewCenter(divName,w,h,url)
{
  var x = (document.body.offsetWidth / 2) - (w/2);
  var y = (document.body.offsetHeight / 2) - (h/2);
  PopView(divName,x,y,w,h,url);
}

//20070419 Luther Add ---START---
//add change channel then change the title.
function ChangeChannelTextPanel(id)
{
  var obj = GE("chtxt");
  if (obj != null)
  {
    if (id <= 0)
    {
      id = 1;
    }
    obj.innerHTML = GL("channel") +" "+ id;
  }
}
function GetChannelTextPanelHTML(id)
{
  if (id <= 0)
  {
    id = 1;
  }
  return "<span id='chtxt'>"+GL("channel")+" "+id+"</span>";
}
//20070419 Luther add --- END ---

//20070620 Luther Add ---START---
//add change channel then change the title.
function IsEarlyIpCam()
{
  return (g_machineCode == "1688" || //7214
           g_machineCode == "1788" || //7215
           g_machineCode == "5678" || //7211,7221
           g_machineCode == "5679" || //7211W
		   g_machineCode == "1679");} //7313

//20070620 Luther add --- END ---
//20080204 Luther add ---START---
function GetCookie(name)
{
  var cname= name+"=";
  var dc = document.cookie;
  if(dc.length>0)
  {
    begin = dc.indexOf(cname);
    if(begin!=-1)
    {
      begin=begin+cname.length;
      end=dc.indexOf(";",begin);
      if(end==-1)
      {
        end=dc.length;
      }
      return dc.substring(begin,end);
    }
  }
  return null;
};

function SetCookie(name, value, expires)
{
  CookiesExpDay="365";
  var time = new Date();
  time.setTime(time.getTime()+((86400000)*CookiesExpDay));
  if(expires==null)
  {expires="";}
  document.cookie = name+"="+value+";expires="+ time.toGMTString();
};
function SaveCombo(id,cookieName)
{
  var obj = GE(id);
  if (obj != null)
  {
    var ix = obj.selectedIndex;
    if (ix >=0)
    {
      SetCookie(cookieName,obj.options[obj.selectedIndex].value);
    }
  }
};
function GetCookieInt(name,def)
{
  var i = parseInt(GetCookie(name));
  if (isNaN(i))
  {
    i = def;
  }
  return i;
}
function GetCookieStr(name,def)
{
  var i = GetCookie(name);
  if (i == null)
  {
    i = def;
  }
  return i;
}

//20080204 Luther add --- END ---
/*
var g_p1XSize=parseInt(GV("<%profile1xsize%>",320));
var g_p1YSize=parseInt(GV("<%profile1ysize%>",240));
var g_p2XSize=parseInt(GV("<%profile2xsize%>",320));
var g_p2YSize=parseInt(GV("<%profile2ysize%>",240));
var g_p3XSize=parseInt(GV("<%profile3xsize%>",320));
var g_p3YSize=parseInt(GV("<%profile3ysize%>",240));

var g_isSupP1=(parseInt(GV("<%supportprofile1%>",0)) >= 1);
var g_isSupP2=(parseInt(GV("<%supportprofile2%>",0)) >= 1);
var g_isSupP3=(parseInt(GV("<%supportprofile3%>",0)) >= 1);
*/
function UpdateGSize(z)
{
  //1
  if (g_isSupP1 && z==1)
  {
    g_viewXSize = g_p1XSize;
    g_viewYSize = g_p1YSize;
  }
  else if (z==1)
  {
    z=2;
  }
  //2
  if (g_isSupP2 && z==2)
  {
    g_viewXSize = g_p2XSize;
    g_viewYSize = g_p2YSize;
  }
  else if (z==2)
  {
    z=3;
  }
  //3
  if (g_isSupP3 && z==3)
  {
    g_viewXSize = g_p3XSize;
    g_viewYSize = g_p3YSize;
  }
  else if (z==3)
  {
    z=4;
  }
  
  //4
  if (g_isSupP4 && z==4)
  {
    g_viewXSize = g_p4XSize;
    g_viewYSize = g_p4YSize;
  }
  else if (z==4)
  {
    z=1;
  }
  //alert(g_viewXSize+":"+g_viewYSize);
  //alert(g_isSupP1+":"+g_isSupP2+":"+g_isSupP3+":"+z);
  return z;
}

function GetJpegMpeg4Ctx(index,useActiveX,onChang)
{
  var o='';
  o+='<select onChange='+onChang+' id='+((useActiveX==1)?'JpegMpeg4Chg':'AJAXJpegChg')+' class="m1">';
  if (g_isSupS1)
    o+='<option value="1" '+((index==1)?'selected':'')+' >'+g_stream1name+'</option>';
  if (g_isSupS2)
    o+='<option value="2" '+((index==2)?'selected':'')+' >'+g_stream2name+'</option>';
  if (g_isSupS3 && useActiveX == 1)
    o+='<option value="3" '+((index==3)?'selected':'')+' >'+g_stream3name+'</option>';
  if (g_isSupS4 && useActiveX == 1)
    o+='<option value="4" '+((index==4)?'selected':'')+' >'+g_stream4name+'</option>';
  o+='</select>';
  return o;
};
function CheckBrowser()
{ 
  var cb = "Unknown"; 
  if(window.ActiveXObject)
  { 
    cb = "IE"; 
  }
  else if(navigator.userAgent.toLowerCase().indexOf("firefox") != -1)
  { 
    cb = "Firefox"; 
  }
  else if((typeof document.implementation != "undefined") && (typeof document.implementation.createDocument != "undefined") && (typeof HTMLDocument != "undefined"))
  { 
    cb = "Mozilla"; 
  }
  else if(navigator.userAgent.toLowerCase().indexOf("opera") != -1)
  { 
      cb = "Opera"; 
  } 
  return cb; 
};
function IsNumericRange(sText,start,end)
{
  var ValidChars = "0123456789."; 
  var IsNumber=true; 
  var Char; 
  for (i = 0; i < sText.length && IsNumber == true; i++) 
  { 
    Char = sText.charAt(i); 
    if (ValidChars.indexOf(Char) == -1) 
    { 
      return false; 
    } 
  }
  if(start!=null)
  {
    sText=parseInt(sText);
    start=parseInt(start);
    end=parseInt(end);
    if(sText<start || sText>end)
    {
      return false; 
    }
  }	
  
  return IsNumber;   
}

function ShowDiv(oRad,id)   
{   
  var Layer_choice;     
  if (document.getElementById) 
  { //Netscape 6.x   
    Layer_choice = eval("document.getElementById(\" "+id+" \")");   
  }   
  else 
  { // IE 5.x   
    Layer_choice = eval(" \"document.all.choice."+id+" \" ");   
  }   
  
  if(Layer_choice)
  {   
    if(oRad == true)   
    {  
       Layer_choice.style.display='';   
    }
    else  
    {   
       Layer_choice.style.display='none';   
    }   
  }   
};

function Ctrl_SelectEx(id,list,val,setcmd,onChangeFunc,checker,inactive)
{
  this.type = "select";
  this.active = !(inactive==true);
  this.id = id;
  this.list = list;
  this.value = val;
  this.setcmd = setcmd;
  this.checker = checker;
  this.html = SelectObjectNoWriteEx(id,list,val,onChangeFunc);
  this.GV = function (){ return GetValue(this.id); };
  this.SV = function (val){ SetValue(this.id,val); };
  this.IsPass = function (isQuite){ return CPASS(this,isQuite);};
};

function SelectObjectNoWriteEx(strName,strOption,intValue,onChange)
{
  var o='';
  o+='<SELECT NAME="'+strName+'" id="'+strName+'" class="m1"';
  if (onChange == null)
  {
    o+='>';
  }
  else
  {
    o+=' onChange="'+onChange+'" >';
  }
  aryOption = strOption.split(';');
  for (var i = 0; i<aryOption.length; i++)
  {
    if(aryOption[i]==intValue)
    {
      o+='<OPTION selected value="'+aryOption[i]+'">'+aryOption[i];
    }
    else
    {
      o+='<OPTION value="'+aryOption[i]+'">'+aryOption[i];
    }
  }
  o+='</SELECT>';
  return o;
};

function CheckIpFormat(val,msg,isQuite)
{
  var result = false;
  var valArray = val.split('.');
  if(valArray.length != 4)
  {
    alert(GL(msg));
  }
  else
  {
    for(i=0;i<4;i++)
	{
	  if(!IsNum(valArray[i]))
	  {
	    alert(GL(msg));
		return false;
	  }	
	  if(!(parseInt(valArray[i]) >=0 && parseInt(valArray[i]) <=255))
	  {
	    alert(GL(msg));
		return false;
	  }	
	}
	result = true;
  }
  return result;
};

function IsNum(id)
{  
  var tmp;
  var z="0123456789";
  var nab=id.length-1;
  for (var i=0;i<=nab;i++)
  {  
    tmp=id.substr(i,1);
    if (z.indexOf(tmp) == -1) { return false; }  
  }  
  return true;
};
if(g_isIpcam)
  var g_str = GL("ipcam_str"); 
else
  var g_str = GL("videoserver_str");
