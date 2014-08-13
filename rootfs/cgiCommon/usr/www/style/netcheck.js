
function pingstart() 
{
	this.po=window.location.protocol+"//"+window.location.host+"/cgi-bin/ping.cgi?now=" + new Date().getTime(); 
	this.init = function() 
	{
		var request;
		try { request = new XMLHttpRequest(); } 
		catch(e1) 
		{
			try { request = new ActiveXObject("Msxml2.XMLHTTP"); } 
			catch(e2) 
			{
				try { request = new ActiveXObject("Microsoft.XMLHTTP"); } 
				catch(e3) { return; }
			}
		};
		request.open("GET",this.po,true); 
		request.send(null);
	};
	this.init(); 
}

function getpingresult() 
{
	this.result = "";
	this.po=window.location.protocol+"//"+window.location.host+"/ping.xml?now=" + new Date().getTime(); 
	this.init = function() 
	{
		var request;
		try { request = new XMLHttpRequest(); } 
		catch(e1) 
		{
			try { request = new ActiveXObject("Msxml2.XMLHTTP"); } 
			catch(e2) 
			{
				try { request = new ActiveXObject("Microsoft.XMLHTTP"); } 
				catch(e3) { return; }
			}
		};
		request.open("GET",this.po,false); 
		request.send(null);
		if(request.status==200) 
		{
			this.result = request.responseText;
		}
	};
	this.init(); 
}

function getPPPoEresult() 
{
	this.result = "";
	this.po=window.location.protocol+"//"+window.location.host+"/pppoe_status.xml?now=" + new Date().getTime(); 
	this.init = function() 
	{
		var request;
		try { request = new XMLHttpRequest(); } 
		catch(e1) 
		{
			try { request = new ActiveXObject("Msxml2.XMLHTTP"); } 
			catch(e2) 
			{
				try { request = new ActiveXObject("Microsoft.XMLHTTP"); } 
				catch(e3) { return; }
			}
		};
		request.open("GET",this.po,false); 
		request.send(null);
		if(request.status==200) 
		{
			this.result = request.responseText;
		}
	};
	this.init(); 
}
