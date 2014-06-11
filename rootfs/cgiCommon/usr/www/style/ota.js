var xmlHttp;

function check(num)
{
	xmlHttp=GetXmlHttpObject();
	if (xmlHttp==null)
	{
		alert ("Browser does not support HTTP Request");
		Return;
	}
	var url="/cgi-bin/ota.cgi";
	if(num == 1)
	{
		url=url+"?q=check";	
		xmlHttp.onreadystatechange=stateChanged1;
	}
	else if(num == 2)
	{
		url=url+"?q=up";
		xmlHttp.onreadystatechange=stateChanged2;
	}
	
	url=url+"&sid="+Math.random();
	xmlHttp.open("GET",url,true);
	xmlHttp.send(null);
}
function stateChanged1()
{
	if (xmlHttp.readyState==4 || xmlHttp.readyState=="complete")
	{

	}
}
function stateChanged2()
{
	if (xmlHttp.readyState==4 || xmlHttp.readyState=="complete")
	{

	}
}
function GetXmlHttpObject()
{var xmlHttp=null;try{xmlHttp=new XMLHttpRequest();}catch (e){try{xmlHttp=new ActiveXObject("Msxml2.XMLHTTP");}	catch (e){xmlHttp=new ActiveXObject("Microsoft.XMLHTTP");}}return xmlHttp;}
