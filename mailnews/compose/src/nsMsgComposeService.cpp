/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsMsgComposeService.h"
#include "nsMsgCompCID.h"
#include "nsISupportsArray.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsXPIDLString.h"
#include "nsIMsgIdentity.h"
#include "nsISmtpUrl.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsMsgI18N.h"
#include "nsIMsgDraft.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kMsgComposeCID, NS_MSGCOMPOSE_CID);
static NS_DEFINE_CID(kMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID);
static NS_DEFINE_CID(kMsgDraftCID, NS_MSGDRAFT_CID);

nsMsgComposeService::nsMsgComposeService()
{
	nsresult rv;

	NS_INIT_REFCNT();
    rv = NS_NewISupportsArray(getter_AddRefs(m_msgQueue));
}


nsMsgComposeService::~nsMsgComposeService()
{
}

// Utility function to open a message compose window and pass an argument string to it.
static nsresult openWindow( const PRUnichar *chrome, const PRUnichar *args ) {
    nsCOMPtr<nsIDOMWindow> hiddenWindow;
    JSContext *jsContext;
    nsresult rv;
    NS_WITH_SERVICE( nsIAppShellService, appShell, kAppShellServiceCID, &rv )
    if ( NS_SUCCEEDED( rv ) ) {
        rv = appShell->GetHiddenWindowAndJSContext( getter_AddRefs( hiddenWindow ),
                                                    &jsContext );
        if ( NS_SUCCEEDED( rv ) ) {
            // Set up arguments for "window.openDialog"
            void *stackPtr;
            jsval *argv = JS_PushArguments( jsContext,
                                            &stackPtr,
                                            "WssW",
                                            chrome,
                                            "_blank",
                                            "chrome,dialog=no,all",
                                            args );
            if ( argv ) {
                nsCOMPtr<nsIDOMWindow> newWindow;
                rv = hiddenWindow->OpenDialog( jsContext,
                                               argv,
                                               4,
                                               getter_AddRefs( newWindow ) );
                JS_PopArguments( jsContext, stackPtr );
            }
        }
    }
    return rv;
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS2(nsMsgComposeService, nsIMsgComposeService, nsIContentHandler);

nsresult nsMsgComposeService::OpenComposeWindow(const PRUnichar *msgComposeWindowURL, const PRUnichar *originalMsgURI,
	MSG_ComposeType type, MSG_ComposeFormat format, nsIMsgIdentity * identity)
{
	nsAutoString args = "";
	nsresult rv;
	
	/* Actually, the only way to implement forward inline is to simulate a template message. 
	   Maybe one day when we will have more time we can change that
	*/
	if (type == nsIMsgCompType::ForwardInline)
	{
	    nsCOMPtr<nsIMsgDraft> pMsgDraft;
	    rv = nsComponentManager::CreateInstance(kMsgDraftCID,
	                                 nsnull,
	                                 nsCOMTypeInfo<nsIMsgDraft>::GetIID(), 
	                                 getter_AddRefs(pMsgDraft));
	    if (NS_SUCCEEDED(rv) && pMsgDraft)
	    	rv = pMsgDraft->OpenDraftMsg(originalMsgURI, nsnull, identity, PR_TRUE);

		return rv;
	}

	args.Append("type=");
	args.Append(type);

	args.Append(",format=");
	args.Append(format);

	if (identity) {
		nsXPIDLCString key;
		rv = identity->GetKey(getter_Copies(key));
		if (NS_SUCCEEDED(rv) && key && (PL_strlen(key) > 0)) {
			args.Append(",preselectid=");
			args.Append(key);
		}
	}

	if (originalMsgURI && *originalMsgURI)
	{
		if (type == nsIMsgCompType::NewsPost) {
			args.Append(",newsgroups=");
			args.Append(originalMsgURI);
		}
		else {
			args.Append(",originalMsg='");
			args.Append(originalMsgURI);
			args.Append("'");
		}
	}
	
	if (msgComposeWindowURL && *msgComposeWindowURL)
        rv = openWindow( msgComposeWindowURL, args.GetUnicode() );
	else
        rv = openWindow( nsString("chrome://messengercompose/content/").GetUnicode(),
                         args.GetUnicode() );
	
	return rv;
}

NS_IMETHODIMP nsMsgComposeService::OpenComposeWindowWithURI(const PRUnichar * aMsgComposeWindowURL, nsIURI * aURI)
{
  nsresult rv = NS_OK;
  if (aURI)
  { 
    nsCOMPtr<nsIMailtoUrl> aMailtoUrl;
    rv = aURI->QueryInterface(NS_GET_IID(nsIMailtoUrl), getter_AddRefs(aMailtoUrl));
    if (NS_SUCCEEDED(rv))
    {
       PRBool aPlainText = PR_FALSE;
       nsXPIDLCString aToPart;
       nsXPIDLCString aCcPart;
       nsXPIDLCString aBccPart;
       nsXPIDLCString aSubjectPart;
       nsXPIDLCString aBodyPart;
       nsXPIDLCString aAttachmentPart;
       nsXPIDLCString aNewsgroup;

       aMailtoUrl->GetMessageContents(getter_Copies(aToPart), getter_Copies(aCcPart), 
                                    getter_Copies(aBccPart), nsnull /* from part */,
                                    nsnull /* follow */, nsnull /* organization */, 
                                    nsnull /* reply to part */, getter_Copies(aSubjectPart),
                                    getter_Copies(aBodyPart), nsnull /* html part */, 
                                    nsnull /* a ref part */, getter_Copies(aAttachmentPart),
                                    nsnull /* priority */, getter_Copies(aNewsgroup), nsnull /* host */,
                                    &aPlainText);

       MSG_ComposeFormat format = nsIMsgCompFormat::Default;
       if (aPlainText)
         format = nsIMsgCompFormat::PlainText;

       //ugghh more conversion work!!!!
       nsAutoString uniToPart((const char*)aToPart);
       nsAutoString uniCcPart((const char*)aCcPart);
       nsAutoString unicBccPart((const char*)aBccPart);
       nsAutoString uniNewsgroup((const char*)aNewsgroup);
       nsAutoString uniSubjectPart((const char*)aSubjectPart);
       nsAutoString uniBodyPart((const char*)aBodyPart);
       nsAutoString uniAttachmentPart((const char*)aAttachmentPart);
       
       rv = OpenComposeWindowWithValues(aMsgComposeWindowURL,
       									nsIMsgCompType::MailToUrl,
       									format,
       									uniToPart.GetUnicode(), 
                                        uniCcPart.GetUnicode(),
                                        unicBccPart.GetUnicode(), 
                                        uniNewsgroup.GetUnicode(), 
                                        uniSubjectPart.GetUnicode(),
                                        uniBodyPart.GetUnicode(), 
                                        uniAttachmentPart.GetUnicode(),
                                        nsnull);
    }
  }

  return rv;
}

nsresult nsMsgComposeService::OpenComposeWindowWithValues(const PRUnichar *msgComposeWindowURL,
														  MSG_ComposeType type,
														  MSG_ComposeFormat format,
														  const PRUnichar *to,
														  const PRUnichar *cc,
														  const PRUnichar *bcc,
														  const PRUnichar *newsgroups,
														  const PRUnichar *subject,
														  const PRUnichar *body,
														  const PRUnichar *attachment,
														  nsIMsgIdentity *identity)
{
	nsresult rv;
	nsCOMPtr<nsIMsgCompFields> pCompFields;
    rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID,
                                 nsnull,
                                 nsCOMTypeInfo<nsIMsgCompFields>::GetIID(), 
                                 getter_AddRefs(pCompFields));
    if (NS_SUCCEEDED(rv) && pCompFields)
    {
		if (to)			{pCompFields->SetTo(to);}
		if (cc)			{pCompFields->SetCc(cc);}
		if (bcc)		{pCompFields->SetBcc(bcc);}
		if (newsgroups)	{pCompFields->SetNewsgroups(newsgroups);}
		if (subject)	{pCompFields->SetSubject(subject);}
		if (attachment)	{pCompFields->SetAttachments(attachment);}
		if (body)		{pCompFields->SetBody(body);}
	
		rv = OpenComposeWindowWithCompFields(msgComposeWindowURL, type, format, pCompFields, identity);
    }
    
    return rv;
}

nsresult nsMsgComposeService::OpenComposeWindowWithCompFields(const PRUnichar *msgComposeWindowURL,
														  MSG_ComposeType type,
														  MSG_ComposeFormat format,
														  nsIMsgCompFields *compFields,
														  nsIMsgIdentity *identity)
{
	nsAutoString args = "";
	nsresult rv;

	args.Append("type=");
	args.Append(type);

	args.Append(",format=");
	args.Append(format);
	
	if (compFields)
	{
		NS_ADDREF(compFields);
		args.Append(",fieldsAddr="); args.Append((PRInt32)compFields, 10);
	}

	if (identity) {
		nsXPIDLCString key;
		rv = identity->GetKey(getter_Copies(key));
		if (NS_SUCCEEDED(rv) && key && (PL_strlen(key) > 0)) {
			args.Append(",preselectid=");
			args.Append(key);
		}
	}
	if (msgComposeWindowURL && *msgComposeWindowURL)
        rv = openWindow( msgComposeWindowURL, args.GetUnicode() );
	else
        rv = openWindow( nsString("chrome://messengercompose/content/").GetUnicode(),
                         args.GetUnicode() );

    if (NS_FAILED(rv))
		NS_IF_RELEASE(compFields);
    	
	return rv;
}

nsresult nsMsgComposeService::InitCompose(nsIDOMWindow *aWindow,
                                          const PRUnichar *originalMsgURI,
                                          PRInt32 type,
                                          PRInt32 format,
                                          PRInt32 compFieldsAddr,
                                          nsIMsgIdentity *identity,
                                          nsIMsgCompose **_retval)
{
	nsresult rv;
	nsIMsgCompose * msgCompose = nsnull;
	
	rv = nsComponentManager::CreateInstance(kMsgComposeCID, nsnull,
	                                        nsCOMTypeInfo<nsIMsgCompose>::GetIID(),
	                                        (void **) &msgCompose);
	if (NS_SUCCEEDED(rv) && msgCompose)
	{
// ducarroz: I am not quite sure than dynamic_cast is supported on all platforms/compilers!
//		nsIMsgCompFields* compFields = dynamic_cast<nsIMsgCompFields *>((nsIMsgCompFields *)compFieldsAddr);
		nsIMsgCompFields* compFields = (nsIMsgCompFields *)compFieldsAddr;
		msgCompose->Initialize(aWindow, originalMsgURI, type, format,
                           compFields, identity);
		NS_IF_RELEASE(compFields);
		m_msgQueue->AppendElement(msgCompose);
		*_retval = msgCompose;
	}
	
	return rv;
}


nsresult nsMsgComposeService::DisposeCompose(nsIMsgCompose *compose, PRBool closeWindow)
{
	PRInt32 i = m_msgQueue->IndexOf(compose);
	if (i >= 0)
	{
		m_msgQueue->RemoveElementAt(i);
		
    // rhp: Commenting out for now to cleanup compile warning...
		// if (closeWindow)
			;//TODO


		// comment copied from nsMessenger.cpp. It's the same issue.
    // ** clean up
    // *** jt - We seem to have one extra ref count. I have no idea where it
    // came from. This could be the global object we created in commandglue.js
    // which causes us to have one more ref count. Call Release() here
    // seems the right thing to do. This gurantees the nsMessenger instance
    // gets deleted after we close down the messenger window.
    
    // smfr the one extra refcount is the result of a bug 8555, which I have 
    // checked in a fix for. So I'm commenting out this extra release.
		//NS_RELEASE(compose);
	}
	return NS_OK;
}

NS_IMETHODIMP nsMsgComposeService::HandleContent(const char * aContentType, const char * aCommand,
                                                const char * aWindowTarget, nsIChannel * aChannel)
{
  nsresult rv = NS_OK;
  if (aChannel)
  {
    // First of all, get the content type and make sure it is a content type we know how to handle!
    if (nsCRT::strcasecmp(aContentType, "x-application-mailto") == 0)
    {
      nsCOMPtr<nsIURI> aUri;
      rv = aChannel->GetURI(getter_AddRefs(aUri));
      if (aUri)
         rv = OpenComposeWindowWithURI(nsnull, aUri);
    }
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}
