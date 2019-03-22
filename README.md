ndnSIM (KITE version)
======

This is a modified version of ndnSIM 2.3 (based on commit 0970340d), and provides the whole package for KITE simulations for the ACM ICN 2018 paper.

- Using ndnSIM version of [ndn-cxx](https://github.com/named-data-ndnSIM/ndn-cxx) (commit: 4692ba80).

- Using ndnSIM version of [NFD (modified)](https://github.com/KITE-2018/NFD) (branch: kite).

- Runs with ndnSIM version of [ns3](https://github.com/named-data-ndnSIM/ns-3-dev) (commit: 333e6b05).

ndnSIM
======

[![Build Status](https://travis-ci.org/named-data-ndnSIM/ndnSIM.svg)](https://travis-ci.org/named-data-ndnSIM/ndnSIM)

A new release of [NS-3 based Named Data Networking (NDN) simulator](http://ndnsim.net/)
went through extensive refactoring and rewriting.  The key new features of the new
version:

- Packet format changed to [NDN Packet Specification](http://named-data.net/doc/ndn-tlv/)

- ndnSIM uses implementation of basic NDN primitives from
  [ndn-cxx library (NDN C++ library with eXperimental eXtensions)](http://named-data.net/doc/ndn-cxx/)

  Based on version `0.4.1`

- All NDN forwarding and management is implemented directly using source code of
  [Named Data Networking Forwarding Daemon (NFD)](http://named-data.net/doc/NFD/)

  Based on version `0.4.1-1-g704430c`

- Allows [simulation of real applications](http://ndnsim.net/guide-to-simulate-real-apps.html)
  written against ndn-cxx library

[ndnSIM documentation](http://ndnsim.net)
---------------------------------------------

For more information, including downloading and compilation instruction, please refer to
http://ndnsim.net or documentation in `docs/` folder.
