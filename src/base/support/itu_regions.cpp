/*
 * Copyright (c) 2026 by Thomas Neder (https://github.com/tomneda)
 *
 * DABstar is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the License, or any later version.
 *
 * DABstar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with DABstar. If not, write to the Free Software
 * Foundation, Inc. 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "itu_regions.h"
#include <array>
#include <unordered_map>
#include <QDebug>

using TItuMap = std::unordered_map<u16, ItuTables::SItuEntry>;

struct SRawEntry
{
  u8           ecc;
  u8           countryId;
  const char * ITU_Code;
  const char * Country;
};

// ITU Regions — ETSI TS 101 756 V2.4.1, clause 5.4
// NOTE: Lower case ITU codes indicate no known official ITU code (per standard note).
// Entries with duplicate keys (same ECC + Country Id) are defined that way in the standard itself;
// the first listed entry wins via map::insert semantics.
static constexpr std::array cTable3to7 =
{
  // Table 3: ITU Region 1 (European broadcasting area)
  SRawEntry{ 0xE0, 0x09, "ALB", "Albania"            },
  SRawEntry{ 0xE0, 0x02, "ALG", "Algeria"            },
  SRawEntry{ 0xE0, 0x03, "AND", "Andorra"            },
  SRawEntry{ 0xE4, 0x0A, "ARM", "Armenia"            },
  SRawEntry{ 0xE0, 0x0A, "AUT", "Austria"            },
  SRawEntry{ 0xE3, 0x0B, "AZE", "Azerbaijan"         },
  SRawEntry{ 0xE0, 0x06, "BEL", "Belgium"            },
  SRawEntry{ 0xE3, 0x0F, "BLR", "Belarus"            },
  SRawEntry{ 0xE4, 0x0F, "BIH", "Bosnia Herzegovina" },
  SRawEntry{ 0xE1, 0x08, "BUL", "Bulgaria"           },
  SRawEntry{ 0xE3, 0x0C, "HRV", "Croatia"            },
  SRawEntry{ 0xE1, 0x02, "CYP", "Cyprus"             },
  SRawEntry{ 0xE2, 0x02, "CZE", "Czech Republic"     },
  SRawEntry{ 0xE1, 0x09, "DNK", "Denmark"            }, // E1, 9: shared with Faroe (same key per standard); Denmark wins
  SRawEntry{ 0xE1, 0x09, "DNK", "Faroe"              }, // duplicate key (E1, 9): both use ITU code DNK per standard
  SRawEntry{ 0xE0, 0x0F, "EGY", "Egypt"              },
  SRawEntry{ 0xE4, 0x02, "EST", "Estonia"            },
  SRawEntry{ 0xE1, 0x06, "FNL", "Finland"            },
  SRawEntry{ 0xE1, 0x0F, "F",   "France"             },
  SRawEntry{ 0xE4, 0x0C, "GEO", "Georgia"            },
  SRawEntry{ 0xE0, 0x0D, "D",   "Germany"            }, // E0, D per standard (ITU code is "D", not "DE")
  SRawEntry{ 0xE0, 0x01, "D",   "Germany"            }, // E0, 1 per standard (second country-id for Germany)
  SRawEntry{ 0xE1, 0x0A, "GIB", "Gibraltar"          },
  SRawEntry{ 0xE1, 0x01, "GRC", "Greece"             },
  SRawEntry{ 0xE0, 0x0B, "HNG", "Hungary"            },
  SRawEntry{ 0xE2, 0x0A, "ISL", "Iceland"            },
  SRawEntry{ 0xE1, 0x0B, "IRQ", "Iraq"               },
  SRawEntry{ 0xE3, 0x02, "IRL", "Ireland"            },
  SRawEntry{ 0xE0, 0x04, "ISR", "Israel"             },
  SRawEntry{ 0xE0, 0x05, "I",   "Italy"              },
  SRawEntry{ 0xE1, 0x05, "JOR", "Jordan"             },
  SRawEntry{ 0xE3, 0x0D, "KAZ", "Kazakhstan"         },
  SRawEntry{ 0xE4, 0x07, "---", "Kosovo"             },
  SRawEntry{ 0xE5, 0x03, "KGZ", "Kyrgyzstan"         }, // standard: E5, 3 (was wrongly 0xE4 — false dup with Macedonia)
  SRawEntry{ 0xE3, 0x09, "LVA", "Latvia"             },
  SRawEntry{ 0xE3, 0x0A, "LBN", "Lebanon"            },
  SRawEntry{ 0xE1, 0x0D, "LBY", "Libya"              },
  SRawEntry{ 0xE2, 0x09, "LIE", "Liechtenstein"      },
  SRawEntry{ 0xE2, 0x0C, "LTU", "Lithuania"          }, // standard: E2, C (was missing entirely)
  SRawEntry{ 0xE1, 0x07, "LUX", "Luxembourg"         },
  SRawEntry{ 0xE4, 0x03, "MKD", "Macedonia"          }, // standard: E4, 3 (unique key after Kyrgyzstan corrected to E5)
  SRawEntry{ 0xE0, 0x0C, "MLT", "Malta"              },
  SRawEntry{ 0xE4, 0x01, "MDA", "Moldova"            },
  SRawEntry{ 0xE2, 0x0B, "MCO", "Monaco"             },
  SRawEntry{ 0xE3, 0x01, "MNE", "Montenegro"         },
  SRawEntry{ 0xE2, 0x01, "MRC", "Morocco"            },
  SRawEntry{ 0xE3, 0x08, "HOL", "Netherlands"        }, // ITU code is "HOL" (was "NL" which is ISO 3166-1)
  SRawEntry{ 0xE2, 0x0F, "NOR", "Norway"             },
  SRawEntry{ 0xE0, 0x08, "---", "Palestine"          },
  SRawEntry{ 0xE2, 0x03, "POL", "Poland"             },
  SRawEntry{ 0xE4, 0x08, "POR", "Portugal"           }, // E4, 8: shared with Azores and Madeira per standard; Portugal wins
  SRawEntry{ 0xE4, 0x08, "AZR", "Azores"             }, // duplicate key (E4, 8): standard: E4, 8 (was wrongly 0xE0/0x0B)
  SRawEntry{ 0xE4, 0x08, "MDR", "Madeira"            }, // duplicate key (E4, 8): standard: E4, 8 (was wrongly 0xE2/0x08)
  SRawEntry{ 0xE1, 0x0E, "ROU", "Romania"            },
  SRawEntry{ 0xE0, 0x07, "RUS", "Russian Federation" },
  SRawEntry{ 0xE1, 0x03, "SM",  "San Marino"         }, // ITU code is "SM" (was "SM " with trailing space)
  SRawEntry{ 0xE2, 0x0D, "SRB", "Serbia"             },
  SRawEntry{ 0xE2, 0x05, "SVK", "Slovakia"           },
  SRawEntry{ 0xE4, 0x09, "SVN", "Slovenia"           },
  SRawEntry{ 0xE2, 0x0E, "E",   "Spain"              }, // E2, E: shared with Canary Islands per standard; Spain wins
  SRawEntry{ 0xE2, 0x0E, "CNR", "Canary Islands"     }, // duplicate key (E2, E): standard: E2, E (was wrongly 0xE0/0x0E)
  SRawEntry{ 0xE3, 0x0E, "S",   "Sweden"             },
  SRawEntry{ 0xE1, 0x04, "SUI", "Switzerland"        },
  SRawEntry{ 0xE2, 0x06, "SYR", "Syria"              },
  SRawEntry{ 0xE3, 0x05, "TJK", "Tajikistan"         },
  SRawEntry{ 0xE2, 0x07, "TUN", "Tunisia"            }, // was "Tunesia" (typo)
  SRawEntry{ 0xE3, 0x03, "TUR", "Turkey"             },
  SRawEntry{ 0xE4, 0x0E, "TKM", "Turkmenistan"       },
  SRawEntry{ 0xE4, 0x06, "UKR", "Ukraine"            },
  SRawEntry{ 0xE1, 0x0C, "G",   "United Kingdom"     }, // ITU code is "G" (was "GB" which is ISO 3166-1)
  SRawEntry{ 0xE4, 0x0B, "UZB", "Uzbekistan"         },
  SRawEntry{ 0xE2, 0x04, "CVA", "Vatican"            },


  // Table 4: ITU Region 1 (African broadcasting area)
  // NOTE: Algeria (E0,2), Canary Islands (E2,E), Egypt (E0,F), Libya (E1,D), Morocco (E2,1), Tunisia (E2,7)
  // share keys with Table 3 — Table 3 entries win via map::insert semantics.
  SRawEntry{ 0xD1, 0x0A, "ASC", "Ascension Island"        },
  SRawEntry{ 0xD0, 0x06, "AGL", "Angola"                  },
  SRawEntry{ 0xE0, 0x02, "ALG", "Algeria"                 }, // duplicate key (E0,2): already in Table 3; Table 3 wins
  SRawEntry{ 0xD1, 0x09, "BDI", "Burundi"                 },
  SRawEntry{ 0xD0, 0x0E, "BEN", "Benin"                   },
  SRawEntry{ 0xD0, 0x0B, "BFA", "Burkina Faso"            },
  SRawEntry{ 0xD1, 0x0B, "BOT", "Botswana"                },
  SRawEntry{ 0xD0, 0x01, "CME", "Cameroon"                },
  SRawEntry{ 0xE2, 0x0E, "CNR", "Canary Islands"          }, // duplicate key (E2,E): already in Table 3; Table 3 wins
  SRawEntry{ 0xD0, 0x02, "CAF", "Central African Republic"},
  SRawEntry{ 0xD2, 0x09, "TCD", "Chad"                    },
  SRawEntry{ 0xD0, 0x0C, "COG", "Congo"                   },
  SRawEntry{ 0xD1, 0x0C, "COM", "Comoros"                 },
  SRawEntry{ 0xD1, 0x06, "CPV", "Cape Verde"              },
  SRawEntry{ 0xD2, 0x0C, "CTI", "Cote d'Ivoire"           },
  SRawEntry{ 0xD0, 0x03, "DJI", "Djibouti"                },
  SRawEntry{ 0xE0, 0x0F, "EGY", "Egypt"                   }, // duplicate key (E0,F): already in Table 3; Table 3 wins
  SRawEntry{ 0xD1, 0x0E, "ETH", "Ethiopia"                },
  SRawEntry{ 0xD0, 0x08, "GAB", "Gabon"                   },
  SRawEntry{ 0xD1, 0x03, "GHA", "Ghana"                   },
  SRawEntry{ 0xD1, 0x08, "GMB", "Gambia"                  },
  SRawEntry{ 0xD2, 0x0A, "GNB", "Guinea-Bissau"           },
  SRawEntry{ 0xD0, 0x07, "GNE", "Equatorial Guinea"       },
  SRawEntry{ 0xD0, 0x09, "GUI", "Republic of Guinea"      },
  SRawEntry{ 0xD2, 0x06, "KEN", "Kenya"                   },
  SRawEntry{ 0xD1, 0x02, "LBR", "Liberia"                 },
  SRawEntry{ 0xE1, 0x0D, "LBY", "Libya"                   }, // duplicate key (E1,D): already in Table 3; Table 3 wins
  SRawEntry{ 0xD3, 0x06, "LSO", "Lesotho"                 },
  SRawEntry{ 0xD3, 0x0A, "MAU", "Mauritius"               },
  SRawEntry{ 0xD0, 0x04, "MDG", "Madagascar"              },
  SRawEntry{ 0xD0, 0x0F, "MWI", "Malawi"                  },
  SRawEntry{ 0xD0, 0x05, "MLI", "Mali"                    },
  SRawEntry{ 0xE2, 0x01, "MRC", "Morocco"                 }, // duplicate key (E2,1): already in Table 3; Table 3 wins
  SRawEntry{ 0xD1, 0x04, "MTN", "Mauritania"              },
  SRawEntry{ 0xD2, 0x03, "MOZ", "Mozambique"              },
  SRawEntry{ 0xD2, 0x08, "NGR", "Niger"                   },
  SRawEntry{ 0xD1, 0x0F, "NIG", "Nigeria"                 },
  SRawEntry{ 0xD1, 0x01, "NMB", "Namibia"                 },
  SRawEntry{ 0xD3, 0x05, "RRW", "Rwanda"                  },
  SRawEntry{ 0xD1, 0x05, "STP", "Sao Tome and Principe"   },
  SRawEntry{ 0xD3, 0x08, "SEY", "Seychelles"              },
  SRawEntry{ 0xD1, 0x07, "SEN", "Senegal"                 },
  SRawEntry{ 0xD2, 0x01, "SRL", "Sierra Leone"            },
  SRawEntry{ 0xD2, 0x07, "SOM", "Somalia"                 },
  SRawEntry{ 0xD0, 0x0A, "AFS", "South Africa"            },
  SRawEntry{ 0xD3, 0x0C, "SDN", "Sudan"                   },
  SRawEntry{ 0xD2, 0x05, "SWZ", "Swaziland"               },
  SRawEntry{ 0xD0, 0x0D, "TGO", "Togo"                    },
  SRawEntry{ 0xE2, 0x07, "TUN", "Tunisia"                 }, // duplicate key (E2,7): already in Table 3; Table 3 wins
  SRawEntry{ 0xD1, 0x0D, "TZA", "Tanzania"                },
  SRawEntry{ 0xD2, 0x04, "UGA", "Uganda"                  },
  SRawEntry{ 0xD3, 0x03, "AOE", "Western Sahara"          },
  SRawEntry{ 0xD2, 0x0B, "ZAI", "Zaire"                   },
  SRawEntry{ 0xD2, 0x0E, "ZMB", "Zambia"                  },
  SRawEntry{ 0xD2, 0x0D, "ZAN", "Zanzibar"                },
  SRawEntry{ 0xD2, 0x02, "ZWE", "Zimbabwe"                },

  // Table 5: is not (more) defined

  // Table 6: ITU Region 2 (North and South Americas)
  // NOTE: Brazil, Canada, Mexico each span multiple ECC/CId pairs per the standard.
  // Puerto Rico (A0), USA (A0) and Virgin Islands USA (A0) share identical ECC+CId sets;
  // Puerto Rico (listed first) wins all A0 keys via map::insert semantics.
  SRawEntry{ 0xA2, 0x01, "AIA", "Anguilla"                },
  SRawEntry{ 0xA2, 0x02, "ATG", "Antigua and Barbuda"     },
  SRawEntry{ 0xA2, 0x0A, "ARG", "Argentina"               },
  SRawEntry{ 0xA4, 0x03, "ABW", "Aruba"                   },
  SRawEntry{ 0xA2, 0x0F, "BAH", "Bahamas"                 },
  SRawEntry{ 0xA2, 0x05, "BRB", "Barbados"                },
  SRawEntry{ 0xA2, 0x06, "BLZ", "Belize"                  },
  SRawEntry{ 0xA2, 0x0C, "BER", "Bermuda"                 },
  SRawEntry{ 0xA3, 0x01, "BOL", "Bolivia"                 },
  SRawEntry{ 0xA2, 0x0B, "B",   "Brazil"                  }, // standard: A2,B and A6,3/C/D
  SRawEntry{ 0xA6, 0x03, "B",   "Brazil"                  },
  SRawEntry{ 0xA6, 0x0C, "B",   "Brazil"                  },
  SRawEntry{ 0xA6, 0x0D, "B",   "Brazil"                  },
  SRawEntry{ 0xA1, 0x0B, "CAN", "Canada"                  }, // standard: A1,B/C/D/E
  SRawEntry{ 0xA1, 0x0C, "CAN", "Canada"                  },
  SRawEntry{ 0xA1, 0x0D, "CAN", "Canada"                  },
  SRawEntry{ 0xA1, 0x0E, "CAN", "Canada"                  },
  SRawEntry{ 0xA2, 0x07, "CYM", "Cayman Islands"          },
  SRawEntry{ 0xA3, 0x0C, "CHL", "Chile"                   },
  SRawEntry{ 0xA3, 0x02, "CLM", "Colombia"                },
  SRawEntry{ 0xA2, 0x08, "CTR", "Costa Rica"              },
  SRawEntry{ 0xA2, 0x09, "CUB", "Cuba"                    },
  SRawEntry{ 0xA3, 0x0A, "DMA", "Dominica"                },
  SRawEntry{ 0xA3, 0x0B, "DOM", "Dominican Republic"      },
  SRawEntry{ 0xA2, 0x03, "EQA", "Ecuador"                 },
  SRawEntry{ 0xA4, 0x0C, "SLV", "El Salvador"             },
  SRawEntry{ 0xA2, 0x04, "FLK", "Falkland Islands"        },
  SRawEntry{ 0xA1, 0x0F, "GRL", "Greenland"               },
  SRawEntry{ 0xA3, 0x0D, "GRD", "Grenada"                 },
  SRawEntry{ 0xA2, 0x0E, "GDL", "Guadeloupe"              },
  SRawEntry{ 0xA4, 0x01, "GTM", "Guatemala"               },
  SRawEntry{ 0xA3, 0x0F, "GUY", "Guyana"                  },
  SRawEntry{ 0xA4, 0x0D, "HTI", "Haiti"                   },
  SRawEntry{ 0xA4, 0x02, "HND", "Honduras"                },
  SRawEntry{ 0xA3, 0x03, "JMC", "Jamaica"                 },
  SRawEntry{ 0xA3, 0x04, "MRT", "Martinique"              },
  SRawEntry{ 0xA5, 0x0B, "MEX", "Mexico"                  }, // standard: A5,B/D/E/F
  SRawEntry{ 0xA5, 0x0D, "MEX", "Mexico"                  },
  SRawEntry{ 0xA5, 0x0E, "MEX", "Mexico"                  },
  SRawEntry{ 0xA5, 0x0F, "MEX", "Mexico"                  },
  SRawEntry{ 0xA4, 0x05, "MSR", "Montserrat"              },
  SRawEntry{ 0xA2, 0x0D, "ATN", "Netherlands Antilles"    },
  SRawEntry{ 0xA3, 0x07, "NCG", "Nicaragua"               },
  SRawEntry{ 0xA3, 0x09, "PNR", "Panama"                  },
  SRawEntry{ 0xA3, 0x06, "PRG", "Paraguay"                },
  SRawEntry{ 0xA4, 0x07, "PRU", "Peru"                    },
  SRawEntry{ 0xA0, 0x01, "PTR", "Puerto Rico"             }, // standard: A0,1-9/A/B/D/E (shared with USA and VIR; PTR wins)
  SRawEntry{ 0xA0, 0x02, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x03, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x04, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x05, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x06, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x07, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x08, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x09, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x0A, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x0B, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x0D, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA0, 0x0E, "PTR", "Puerto Rico"             },
  SRawEntry{ 0xA4, 0x0A, "SCN", "St. Kitts"               },
  SRawEntry{ 0xA4, 0x0B, "LCA", "St. Lucia"               },
  SRawEntry{ 0xA6, 0x0F, "SPM", "St. Pierre and Miquelon" },
  SRawEntry{ 0xA5, 0x0C, "VCT", "St. Vincent"             },
  SRawEntry{ 0xA4, 0x08, "SUR", "Suriname"                },
  SRawEntry{ 0xA4, 0x06, "TRD", "Trinidad and Tobago"     },
  SRawEntry{ 0xA3, 0x0E, "TCA", "Turks and Caicos Islands"},
  SRawEntry{ 0xA0, 0x01, "USA", "United States of America"}, // duplicate keys (A0,1-9/A/B/D/E): Puerto Rico wins
  SRawEntry{ 0xA0, 0x02, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x03, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x04, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x05, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x06, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x07, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x08, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x09, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x0A, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x0B, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x0D, "USA", "United States of America"},
  SRawEntry{ 0xA0, 0x0E, "USA", "United States of America"},
  SRawEntry{ 0xA4, 0x09, "URG", "Uruguay"                 },
  SRawEntry{ 0xA4, 0x0E, "VEN", "Venezuela"               },
  SRawEntry{ 0xA5, 0x0F, "VRG", "Virgin Islands (British)" }, // duplicate key (A5,F): Mexico wins
  SRawEntry{ 0xA0, 0x01, "VIR", "Virgin Islands (USA)"    }, // duplicate keys (A0,1-9/A/B/D/E): Puerto Rico wins
  SRawEntry{ 0xA0, 0x02, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x03, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x04, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x05, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x06, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x07, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x08, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x09, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x0A, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x0B, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x0D, "VIR", "Virgin Islands (USA)"    },
  SRawEntry{ 0xA0, 0x0E, "VIR", "Virgin Islands (USA)"    },

  // Table 7: ITU Region 3 (Asia and Pacific)
  SRawEntry{ 0xF0, 0x0A, "AFG", "Afghanistan"          },
  SRawEntry{ 0xF0, 0x09, "ARS", "Saudi Arabia"         },
  SRawEntry{ 0xF0, 0x01, "acc", "Australia"            }, // capital cities (commercial and community) — was "AU" (wrong)
  SRawEntry{ 0xF0, 0x02, "arn", "Australia"            }, // regional New South Wales and ACT
  SRawEntry{ 0xF0, 0x03, "acn", "Australia"            }, // capital cities (national broadcasters)
  SRawEntry{ 0xF0, 0x04, "arq", "Australia"            }, // regional Queensland
  SRawEntry{ 0xF0, 0x05, "arc", "Australia"            }, // regional South Australia and Northern Territory
  SRawEntry{ 0xF0, 0x06, "arw", "Australia"            }, // regional Western Australia
  SRawEntry{ 0xF0, 0x07, "arv", "Australia"            }, // regional Victoria and Tasmania — was duplicate (F0, 6, "arc") (wrong)
  SRawEntry{ 0xF0, 0x08, "arf", "Australia"            }, // regional (future) — was missing
  SRawEntry{ 0xF0, 0x0E, "BHR", "Bahrain"              },
  SRawEntry{ 0xF1, 0x03, "BGD", "Bangladesh"           },
  SRawEntry{ 0xF1, 0x02, "BTN", "Bhutan"               },
  SRawEntry{ 0xF0, 0x0B, "BRM", "Myanmar (Burma)"      },
  SRawEntry{ 0xF1, 0x0B, "BRU", "Brunei Darussalam"    },
  SRawEntry{ 0xF2, 0x03, "CBG", "Cambodia"             },
  SRawEntry{ 0xF0, 0x0C, "CHN", "China"                },
  SRawEntry{ 0xF1, 0x0C, "CLN", "Sri Lanka"            },
  SRawEntry{ 0xF1, 0x05, "FJI", "Fiji"                 },
  SRawEntry{ 0xF1, 0x0F, "HKG", "Hong Kong"            },
  SRawEntry{ 0xF2, 0x05, "IND", "India"                },
  SRawEntry{ 0xF2, 0x0C, "INS", "Indonesia"            },
  SRawEntry{ 0xF1, 0x08, "IRN", "Iran"                 },
  SRawEntry{ 0xE1, 0x0B, "IRQ", "Iraq"                 }, // duplicate key (E1,B): already in Table 3; Table 3 wins
  SRawEntry{ 0xF2, 0x09, "J",   "Japan"                },
  SRawEntry{ 0xF1, 0x01, "KIR", "Kiribati"             },
  SRawEntry{ 0xF0, 0x0D, "KRE", "Korea (North)"        },
  SRawEntry{ 0xF1, 0x0E, "KOR", "Korea (South)"        },
  SRawEntry{ 0xF2, 0x01, "KWT", "Kuwait"               },
  SRawEntry{ 0xF3, 0x01, "LAO", "Laos"                 },
  SRawEntry{ 0xF2, 0x06, "MAC", "Macau"                },
  SRawEntry{ 0xF0, 0x0F, "MLA", "Malaysia"             },
  SRawEntry{ 0xF2, 0x0B, "MLD", "Maldives"             },
  SRawEntry{ 0xF3, 0x0E, "FSM", "Micronesia"           },
  SRawEntry{ 0xF3, 0x0F, "MNG", "Mongolia"             },
  SRawEntry{ 0xF1, 0x07, "NRU", "Nauru"                },
  SRawEntry{ 0xF2, 0x0E, "NPL", "Nepal"                },
  SRawEntry{ 0xF1, 0x09, "NZL", "New Zealand"          },
  SRawEntry{ 0xF1, 0x06, "OMA", "Oman"                 },
  SRawEntry{ 0xF1, 0x04, "PAK", "Pakistan"             },
  SRawEntry{ 0xF3, 0x09, "PNG", "Papua New Guinea"     },
  SRawEntry{ 0xF2, 0x08, "PHL", "Philippines"          },
  SRawEntry{ 0xF2, 0x02, "QAT", "Qatar"                },
  SRawEntry{ 0xF1, 0x0A, "SLM", "Solomon Islands"      },
  SRawEntry{ 0xF2, 0x04, "SMO", "Western Samoa"        },
  SRawEntry{ 0xF2, 0x0A, "SNG", "Singapore"            },
  SRawEntry{ 0xF1, 0x0D, "---", "Taiwan"               }, // no ITU code per standard
  SRawEntry{ 0xF3, 0x02, "THA", "Thailand"             },
  SRawEntry{ 0xF3, 0x03, "TON", "Tonga"                },
  SRawEntry{ 0xF2, 0x0D, "UAE", "United Arab Emirates" },
  SRawEntry{ 0xF2, 0x07, "VTN", "Vietnam"              },
  SRawEntry{ 0xF2, 0x0F, "VUT", "Vanuatu"              },
  SRawEntry{ 0xF3, 0x0B, "YEM", "Yemen"                },
};


ItuTables::ItuTables()
{
  qDebug() << "Initializing ITU tables with" << cTable3to7.size() << "entries";
  for (const auto & te : cTable3to7)
  {
    const u16 key = (static_cast<u16>(te.ecc) << 8) | static_cast<u16>(te.countryId);
    mItuMap.insert({ key, { te.ITU_Code, te.Country } }); // insert keeps first on duplicate key
  }
  qDebug() << "ITU tables initialization complete with" << mItuMap.size() << "unique entries";
}

const ItuTables::SItuEntry & ItuTables::find_ITU_entry(const u8 iEcc, const u8 iCountryId) const
{
  const auto it = mItuMap.find((static_cast<u16>(iEcc) << 8) | static_cast<u16>(iCountryId));
  if (it != mItuMap.end())
  {
    return it->second;
  }

  static SItuEntry failStruct;
  static std::array<char, 32> failStr;
  // snprintf(failStr.data(), failStr.size(), "ECC %02X|CId %02X", iEcc, iCountryId);
  snprintf(failStr.data(), failStr.size(), "---");
  failStruct.ITU_Code = failStr.data();
  failStruct.Country = failStr.data();

  return failStruct;
}
