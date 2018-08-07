# sACN Library for C++ / Qt 5

This library can  be used to receive and transmit Streaming ACN data. Streaming ACN, also called sACN or E1.31 is a protocol to transmit DMX data over the network.
The library uses Qt 5 to be platform independent.

## Author and License

The code was taken from [sACNView](https://github.com/docsteer/sacnview) by **Tom Steer** et al. sACNView was released under the **Apache License** and includes code from Electronic Theater Controls released under the **MIT license**.

The actual commit the code was taken from is [#54110d9](https://github.com/marcusbirkin/sacnview/commit/54110d95c34b65c501dbbae9ae494717aef9fb83) from [PR 149](https://github.com/docsteer/sacnview/pull/149). The only modifications to the code were to replace the dependency to their preferences system by two simple setter methods (sACNRxSocket::setNetworkInterface() and sACNTxSocket::setNetworkInterface()) and some code style changes.

## Building

Add the files of this library to a Qt project to use the library. Qt 5.9.0 or higher is required.

## Usage

### Receive sACN Data

Setting up the listener:

```c++
int universe = 1;  // universe in range 1-63999
QSharedPointer<sACNListener> listener = sACNManager::getInstance()->getListener(universe);
connect(listener.data(), SIGNAL(levelsChanged()), this, SLOT(onLevelsChanged()));
```

Handling the incoming data:

```c++
void onLevelsChanged() {
    int channel = 0;  // channel in range 0-511
    int level = listener->mergedLevels()[channel].level;
    // level is in range 0-255
    // ...
}
```
