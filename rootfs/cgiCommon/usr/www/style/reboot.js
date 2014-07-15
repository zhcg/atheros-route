var xmlHttp;
var desIPreboot=location.hostname;
var destnation;
var timeID;
function  reboothandleStateChange()
{
	if(xmlHttp.readyState==4 || xmlHttp.readyState=="complete")
	{
		//if(xmlHttp.status==200)
		{
			clearTimeout(timeID);
			window.parent.endshow();
			setTimeout(function(){window.top.location.href=destnation;},2000);
		}
	}
}

function rebootRequest()
{
	timeID = setTimeout(function(){rebootRequest();},5000);
	xmlHttp=GetXmlHttpObject();

	if (xmlHttp==null)
	{
		alert ("Browser does not support HTTP Request");
		Return;
	}
	xmlHttp.onreadystatechange=reboothandleStateChange;
	xmlHttp.open("GET","http://"+desIPreboot+"/simpleResponse.xml?now=" + new Date().getTime(),true);
	xmlHttp.send(null);
}

function wait(type)
{
	Butterlate.setTextDomain("other");
	if(type == "reboot")
	{
		window.parent.Demo3(_("REBOOT"));
		window.parent.beginshow(1000);
		destnation = "http://"+location.hostname;
		desIPreboot = location.hostname;
		setTimeout(function(){rebootRequest();},100000);
	}
	else if(type == "upload")
	{
		window.parent.Demo3(_("UPLOAD"));
		window.parent.beginshow(2000);
		destnation = "http://"+desIPreboot+"/cgi-bin/"+"crupload";
		setTimeout(function(){rebootRequest();},200000);
	}
	else if(type == "cfgback")
	{
		window.parent.Demo3(_("CFGBACK"));
		window.parent.beginshow(1000);
		destnation = "http://"+desIPreboot;
		setTimeout(function(){rebootRequest();},100000);
	}
	else if(type == "factory")
	{
		window.parent.Demo3(_("FACTORY"));
		window.parent.beginshow(1000);
		desIPreboot="10.10.10.254";
		destnation = "http://"+desIPreboot+"/cgi-bin/"+"crfact";
		setTimeout(function(){rebootRequest();},100000);
	}
	else
	{
		destnation = "http://"+desIPreboot;
		setTimeout(function(){rebootRequest();},100000);
	}
}

function GetXmlHttpObject()
{var xmlHttp=null;try{
	// Firefox, Opera 8.0+, Safari
		xmlHttp=new XMLHttpRequest();}
	catch (e)
	{		//Internet Explorer
		try	{xmlHttp=new ActiveXObject("Msxml2.XMLHTTP");}
		catch (e){xmlHttp=new ActiveXObject("Microsoft.XMLHTTP");}
	}return xmlHttp;
}
function search(str)
{
	xmlHttp=GetXmlHttpObject();

	if (xmlHttp==null)
	{
		alert ("Browser does not support HTTP Request");
		Return;
	}
	desIPreboot=location.hostname;
	var url="/cgi-bin/reboot.cgi";
	url=url+"?q="+str;
	url=url+"&sid="+Math.random();
	xmlHttp.onreadystatechange=stateChanged;
	xmlHttp.open("GET",url,true);
	xmlHttp.send(null);
	wait(str);
}
function stateChanged(){}
