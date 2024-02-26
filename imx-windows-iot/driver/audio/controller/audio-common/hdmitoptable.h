/* Copyright (c) Microsoft Corporation All Rights Reserved
   Licensed under the MIT License.

Module Name:

    hdmitoptable.h

Abstract:

    Declaration of topology tables for the hdmi endpoint.

--*/

#ifndef _HDMITOPTABLE_H_
#define _HDMITOPTABLE_H_

//=============================================================================
static
KSDATARANGE HdmiTopoPinDataRangesBridge[] =
{
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    },
};

//=============================================================================
static
PKSDATARANGE HdmiTopoPinDataRangePointersBridge[] =
{
  &HdmiTopoPinDataRangesBridge[0],
};

//=============================================================================
static
PCPIN_DESCRIPTOR HdmiTopoMiniportPins[] =
{
    // KSPIN_TOPO_WAVEOUT_SOURCE
    {
        0,
        0,
        0,                                                  // InstanceCount
        NULL,                                               // AutomationTable
        {                                                   // KsPinDescriptor
            0,                                                // InterfacesCount
            NULL,                                             // Interfaces
            0,                                                // MediumsCount
            NULL,                                             // Mediums
            SIZEOF_ARRAY(HdmiTopoPinDataRangePointersBridge), // DataRangesCount
            HdmiTopoPinDataRangePointersBridge,               // DataRanges
            KSPIN_DATAFLOW_IN,                                // DataFlow
            KSPIN_COMMUNICATION_NONE,                         // Communication
            &KSCATEGORY_AUDIO,                                // Category
            NULL,                                             // Name
            0                                                 // Reserved
        }
    },
    // KSPIN_TOPO_LINEOUT_DEST
    {
        0,
        0,
        0,                                                  // InstanceCount
        NULL,                                               // AutomationTable
        {                                                   // KsPinDescriptor
            0,                                                // InterfacesCount
            NULL,                                             // Interfaces
            0,                                                // MediumsCount
            NULL,                                             // Mediums
            SIZEOF_ARRAY(HdmiTopoPinDataRangePointersBridge), // DataRangesCount
            HdmiTopoPinDataRangePointersBridge,               // DataRanges
            KSPIN_DATAFLOW_OUT,                               // DataFlow
            KSPIN_COMMUNICATION_NONE,                         // Communication
            &KSNODETYPE_HDMI_INTERFACE,                       // Category
            NULL,                                             // Name
            0                                                 // Reserved
        }
    }
};

//=============================================================================
static
KSJACK_DESCRIPTION HdmiJackDesc =
{
    KSAUDIO_SPEAKER_STEREO,
    0x0000,               // no color
    eConnTypeOtherDigital,
    eGeoLocHDMI,
    eGenLocPrimaryBox,
    ePortConnJack,
    TRUE
};

// Only return a KSJACK_DESCRIPTION for the physical bridge pin.
static 
PKSJACK_DESCRIPTION HdmiJackDescriptions[] =
{
    NULL,
    &HdmiJackDesc
};

//=============================================================================
static
PCCONNECTION_DESCRIPTOR HdmiTopoMiniportConnections[] =
{
    //  FromNode,      FromPin,                   ToNode,        ToPin
    {   PCFILTER_NODE, KSPIN_TOPO_WAVEOUT_SOURCE, PCFILTER_NODE, KSPIN_TOPO_LINEOUT_DEST}
};


//=============================================================================
static
PCPROPERTY_ITEM PropertiesHdmiTopoFilter[] =
{
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HdmiTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HdmiTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_SINK_INFO,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HdmiTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_CONTAINERID,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_HdmiTopoFilter
    }
};

static PCEVENT_ITEM HdmiJackInfoChangeEvent[] =
{
    {
        &KSEVENTSETID_PinCapsChange,   // Something changed
        KSEVENT_PINCAPS_JACKINFOCHANGE,  // Jack Info Changes
        KSEVENT_TYPE_ENABLE | KSEVENT_TYPE_BASICSUPPORT,
        EventHandler_HdmiTopoFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP_EVENT(AutomationHdmiTopoFilter, PropertiesHdmiTopoFilter, HdmiJackInfoChangeEvent);

//=============================================================================
static
PCFILTER_DESCRIPTOR HdmiTopoMiniportFilterDescriptor =
{
    0,                                            // Version
    &AutomationHdmiTopoFilter,                    // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                     // PinSize
    SIZEOF_ARRAY(HdmiTopoMiniportPins),           // PinCount
    HdmiTopoMiniportPins,                         // Pins
    sizeof(PCNODE_DESCRIPTOR),                    // NodeSize
    0,                                            // NodeCount
    NULL,                                         // Nodes
    SIZEOF_ARRAY(HdmiTopoMiniportConnections),    // ConnectionCount
    HdmiTopoMiniportConnections,                  // Connections
    0,                                            // CategoryCount
    NULL                                          // Categories
};

#endif // _HDMITOPTABLE_H_
    

