
function ajaxget() 
{
	this.seturl = function(url) 
	{ 
		this.po=window.location.protocol+"//"+window.location.host+"/"+url; 
		this.init(); 
	}
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
			this.gettext = request.responseText;
		}
		else
		{
			this.gettext = "fail";
		}
	};
	
}
function ajaxget_2() 
{
	this.seturl = function(url) 
	{ 
		this.po=window.location.protocol+"//"+window.location.host+"/"+url; 
	}
	this.init = function(xcb) 
	{
		try { this.request = new XMLHttpRequest(); } 
		catch(e1) 
		{
			try { this.request = new ActiveXObject("Msxml2.XMLHTTP"); } 
			catch(e2) 
			{
				try { this.request = new ActiveXObject("Microsoft.XMLHTTP"); } 
				catch(e3) { return; }
			}
		};
		this.request.onreadystatechange=xcb;
		this.request.open("GET",this.po,true); 
		this.request.send(null);
		
	};
	
}