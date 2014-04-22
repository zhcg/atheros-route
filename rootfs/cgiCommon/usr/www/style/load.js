var t_DiglogX,t_DiglogY,t_DiglogW,t_DiglogH;
function DialogShow(showdata,ow,oh,w,h)
{  //显示DIV置顶层
  var objDialog = document.getElementById("DialogMove");
  if (!objDialog) 
  objDialog = document.createElement("div");
  t_DiglogW = ow;
  t_DiglogH = oh;
  DialogLoc();
  objDialog.id = "DialogMove";
  var oS = objDialog.style;
  oS.display = "block";
  oS.top = t_DiglogY + "px";
  oS.left = t_DiglogX + "px";
  oS.margin = "0px";
  oS.padding = "0px";
  oS.width = w + "px";
  oS.height = h + "px";
  oS.position = "absolute";
  oS.zIndex = "5";
  oS.background = "#ffffff";
  oS.border = "solid #000 0px";
  objDialog.innerHTML = showdata;
  document.body.appendChild(objDialog);
}
function DialogHide()
{  //关闭div置顶层的主调
  ScreenClean();
  var objDialog = document.getElementById("DialogMove");
  if (objDialog)
  objDialog.style.display = "none";
}
function DialogLoc()
{  //计算div窗口位置
  var dde = document.documentElement;
  if (window.innerWidth)
  {
  var ww = window.innerWidth;
  var wh = window.innerHeight;
  var bgX = window.pageXOffset;
  var bgY = window.pageYOffset;
  }
  else
  {
  var ww = dde.offsetWidth;
  var wh = dde.offsetHeight;
  var bgX = dde.scrollLeft;
  var bgY = dde.scrollTop;
  }
  t_DiglogX = (bgX + ((ww - t_DiglogW)/2));
  t_DiglogY = (bgY + ((wh - t_DiglogH)/2));
}
function ScreenConvert()
{  //整个页面区域加屏蔽层
  var objScreen = document.getElementById("ScreenOver");
  if(!objScreen) 
  var objScreen = document.createElement("div");
  var oS = objScreen.style;
  objScreen.id = "ScreenOver";
  oS.display = "block";
  oS.top = oS.left = oS.margin = oS.padding = "0px";
  if (document.body.clientHeight) 
  {
  var wh = document.body.clientHeight + "px";
  }
  else if (window.innerHeight)
  {
  var wh = window.innerHeight + "px";
  }
  else
  {
  var wh = "100%";
  }
  oS.width = "100%";
  oS.height = document.body.scrollHeight+"px";
  oS.position = "absolute";
  oS.zIndex = "3";
  oS.background = "#cccccc";
  oS.filter = "alpha(opacity=50)";
  oS.opacity = 40/100;
  oS.MozOpacity = 40/100;
  document.body.appendChild(objScreen);
  var allselect = document.getElementsByTagName("select");
  for (var i=0; i<allselect.length; i++) 
  allselect[i].style.visibility = "hidden";
}
function ScreenClean(){  //清屏
  var objScreen = document.getElementById("ScreenOver");
  if (objScreen)
  objScreen.style.display = "none";
  var allselect = document.getElementsByTagName("select");
  for (var i=0; i<allselect.length; i++) 
  allselect[i].style.visibility = "visible";
}
function Demo(string)
{ //主调
  ScreenConvert();
  var ShowDiv="<div style=\"padding:10px;width:200px;height:120;border:#909090 1px solid;background:#fff;color:#333;filter:progid:DXImageTransform.Microsoft.Shadow(color=#909090,direction=120,strength=4);-moz-box-shadow: 8px 8px 18px #909090;-webkit-box-shadow: 8px 8px 18px #909090;box-shadow:8px 8px 18px #909090;\"><marquee style=\"border:1px solid #000000\" direction=\"right\" width=\"180\" scrollamount=\"5\" scrolldelay=\"5\" bgcolor=\"#ECF2FF\"\><table cellspacing=\"1\" cellpadding=\"0\"><tr height=15><td bgcolor=#a3a4a5 width=15></td><td></td><td bgcolor=#a3a4a5 width=15></td><td></td><td bgcolor=#a3a4a5 width=15></td><td></td><td bgcolor=#a3a4a5 width=15></td><td></td></tr></table></marquee><br><br><div style=\"color:#333\" align=\"center\">"+string+"</div></div>";
  DialogShow(ShowDiv,100,50,200,120);
}
function Demo2(string){ //主调
  ScreenConvert();
  var ShowDiv="<div style=\"text-align: center;padding:10px;width:200px;height:120;border:#909090 1px solid;background:#fff;color:#333;filter:progid:DXImageTransform.Microsoft.Shadow(color=#909090,direction=120,strength=4);-moz-box-shadow: 8px 8px 18px #909090;-webkit-box-shadow: 8px 8px 18px #909090;box-shadow:8px 8px 18px #909090;\"><span style=\"color:#333;font-size:18px;line-height:120px;\">"+string+"<span></div>";
  DialogShow(ShowDiv,100,50,200,120);
}

function Demo3(string)
{ //主调
  ScreenConvert();
  var ShowDiv="<div style=\"border:#909090 1px solid;height:120;\"><br><div style=\"border:1px #CCC solid; height:20px; width:198px; margin:0 auto;\"><div id=\"demshow\" style=\" background:#909090;height:20px; width:0\"></div><strong id=\"devval\" style=\" position:absolute; width:198px; top:20px; text-align:center;color:#333; overflow:hidden\">0%</strong></div><br><div style=\"color:#333\" align=\"center\">"+string+"</div><br></div>";
  DialogShow(ShowDiv,100,50,200,120);
}
var pxi = 0;var timer1;var timer2;

function showpxb()
{
	document.getElementById("demshow").style.width=pxi.toString()+"%";
	document.getElementById("devval").innerHTML=pxi.toString()+"%";
	pxi++;
	if(pxi==91)
	{
		clearInterval(timer1);
	}
	
}
function beginshow(tim)
{
	timer1=setInterval('showpxb()',tim);
}

function showpxe()
{
	document.getElementById("demshow").style.width=pxi.toString()+"%";
	document.getElementById("devval").innerHTML=pxi.toString()+"%";
	pxi++;
	if(pxi==101)
	{
		clearInterval(timer2);
	}
	
}
function endshow()
{
	pxi=90;
	timer2=setInterval('showpxb()',1000);
}

